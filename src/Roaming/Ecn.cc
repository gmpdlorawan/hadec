#include <chrono>
#include "Ecn.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include "inet/linklayer/common/MacAddress.h"

namespace flora {

Define_Module(Ecn);

void Ecn::initialize(int stage)
{
    if (stage == 0) {
        strategy = par("strategy").stdstringValue();
        if (strlen(par("calibFile").stringValue()) > 0) calib = fopen(par("calibFile").stringValue(), "w");
        cStringTokenizer tok(par("serverPaths").stringValue());
        while (tok.hasMoreTokens()) serverPaths.push_back(tok.nextToken());
        cStringTokenizer tg(par("gwNets").stringValue());
        while (tg.hasMoreTokens()) gwNet.push_back(atoi(tg.nextToken()));
    }
    else if (stage == 1) {
        cModule *net = getSimulation()->getSystemModule();
        for (int i = 0; i < (int)gwNet.size(); i++) {
            cModule *gw = net->getSubmodule("loRaGW", i);
            cModule *mob = gw->getSubmodule("mobility");
            gwPos.push_back({mob->par("initialX").doubleValue(), mob->par("initialY").doubleValue()});
        }
        if (strategy == "oracle")
            loadTraces();
    }
}

void Ecn::loadTraces()
{
    std::string file = par("traceFile").stdstringValue();
    if (file.empty())
        throw cRuntimeError("oracle strategy needs traceFile");
    std::ifstream in(file);
    if (!in) throw cRuntimeError("cannot open trace file %s", file.c_str());
    cModule *net = getSimulation()->getSystemModule();
    std::string line;
    int idx = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        cModule *node = net->getSubmodule(par("nodesPath").stringValue(), idx);
        if (node == nullptr) break;
        cModule *mac = node->getSubmodule("LoRaNic")->getSubmodule("mac");
        long devId = (long) inet::MacAddress(mac->par("address").stringValue()).getInt();
        std::istringstream ss(line);
        std::vector<Waypoint> w;
        double t, x, y;
        while (ss >> t >> x >> y) w.push_back({t, x, y});
        traces[devId] = w;
        idx++;
    }
}

bool Ecn::positionAt(long devId, double t, double &x, double &y) const
{
    auto it = traces.find(devId);
    if (it == traces.end() || it->second.empty()) return false;
    const auto &w = it->second;
    if (t <= w.front().t) { x = w.front().x; y = w.front().y; return true; }
    if (t >= w.back().t) { x = w.back().x; y = w.back().y; return true; }
    for (size_t i = 1; i < w.size(); i++) {
        if (t <= w[i].t) {
            double a = (t - w[i-1].t) / std::max(1e-9, w[i].t - w[i-1].t);
            x = w[i-1].x + a * (w[i].x - w[i-1].x);
            y = w[i-1].y + a * (w[i].y - w[i-1].y);
            return true;
        }
    }
    return false;
}

double Ecn::phi(double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); }

double Ecn::requiredSnr(int sf)
{
    static const double req[6] = {-7.5, -10, -12.5, -15, -17.5, -20};
    return req[std::min(5, std::max(0, sf - 7))];
}

double Ecn::sensitivity(int sf)
{
    static const double s[6] = {-124, -127, -130, -133, -135, -137};
    return s[std::min(5, std::max(0, sf - 7))];
}

double Ecn::meanRx(int net, double x, double y) const
{
    double best = -1e9;
    double B = par("plB"), n = par("plN"), d0 = par("plD0").doubleValue();
    for (size_t i = 0; i < gwNet.size(); i++) {
        if (gwNet[i] != net) continue;
        double d = std::max(1.0, std::hypot(x - gwPos[i].first, y - gwPos[i].second));
        double p = 14 - (B + 10 * n * std::log10(d / d0));
        best = std::max(best, p);
    }
    return best;
}

int Ecn::bestNetAt(double x, double y) const
{
    return meanRx(1, x, y) > meanRx(0, x, y) ? 1 : 0;
}

void Ecn::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        auto t = check_and_cast<KeyTimer *>(msg);
        evaluate(t->getDevId(), t->getFcnt());
        delete msg;
        return;
    }
    auto m = check_and_cast<BackendMsg *>(msg);
    if (m->getBkType() == BK_OBS) {
        if (counting()) obsReceived++;
        EcnDevice &d = devs[m->getDevId()];
        d.serving = m->getToNet();
        bool first = d.open.find(m->getFcnt()) == d.open.end();
        EcnFrame &f = d.open[m->getFcnt()];
        int net = m->getFromNet();
        if (net >= 0 && net < 2) {
            f.rx[net] = true;
            f.rssi[net] = m->getRssi();
            f.snir[net] = m->getSnir();
        }
        f.fcnt = m->getFcnt();
        f.t = m->getRxTime();
        f.sf = m->getSf();
        f.x = m->getXPos();
        f.y = m->getYPos();
        f.serving = m->getToNet();
        if (first) {
            auto t = new KeyTimer("ecnEval");
            t->setDevId(m->getDevId());
            t->setFcnt(m->getFcnt());
            scheduleAt(simTime() + par("evalDelay"), t);
        }
    }
    else if (m->getBkType() == BK_SWITCH_DONE) {
        EcnDevice &d = devs[m->getDevId()];
        d.serving = m->getToNet();
        d.blockedUntil = simTime().dbl() + par("tBloc").doubleValue();
        d.pendingUntil = -1;
        d.failures = 0;
        d.hyst = 0;
        d.hasEma = false;
        if (counting()) switchesSeen++;
    }
    delete msg;
}

void Ecn::evaluate(long devId, int fcnt)
{
    EcnDevice &d = devs[devId];
    auto it = d.open.find(fcnt);
    if (it == d.open.end()) return;
    EcnFrame f = it->second;
    d.open.erase(it);
    // calibration : comparer la prédiction faite au message précédent avec l'écart observé à ce message
    auto pit = preds.find(&d);
    if (pit != preds.end()) {
        int s0 = pit->second.serving, o0 = 1 - s0;
        if (calib && (f.rx[0] || f.rx[1])) {
            double off = par("censorOffset").doubleValue();
            double pa = f.rx[s0] ? f.rssi[s0] : sensitivity(f.sf) - off;
            double pb = f.rx[o0] ? f.rssi[o0] : sensitivity(f.sf) - off;
            fprintf(calib, "%.5f,%d,%.3f,%d,%d\n", pit->second.conf, (f.rx[0] && f.rx[1]) ? 1 : 0, pb - pa, (pb - pa > pit->second.H) ? 1 : 0, pit->second.n);
        }
        preds.erase(pit);
    }
    d.hist.push_back(f);
    double keep = std::max(par("tauW").doubleValue(), par("v1History").doubleValue()) + 120;
    while (!d.hist.empty() && d.hist.front().t < f.t - keep)
        d.hist.pop_front();

    int o = 1 - f.serving;
    // preparation: as soon as the other network hears the device (signalling only)
    if (strategy != "none" && f.rx[o] && !d.prepared[o] && par("sendPrepare").boolValue()) {
        d.prepared[o] = true;
        auto p = new BackendMsg("Prepare");
        p->setBkType(BK_PREPARE);
        p->setDevId(devId);
        p->setToNet(o);
        if (counting()) preparesSent++;
        delete p;   // profile exchange is not simulated further: counted as signalling
    }
    if (strategy == "none") return;
    double now = simTime().dbl();
    if (now < d.blockedUntil || now < d.pendingUntil) return;
    if (!f.rx[o]) { if (strategy == "hysteresis") d.hyst = 0; return; }

    int rejoinSF = -1;
    lastHeardByOther[devId] = now;
    if (counting()) {
        long near = 0; double tw = par("tauW").doubleValue();
        for (auto &kv : lastHeardByOther) if (kv.second >= now - tw) near++;
        nearSum += near; nearSamples++; nearMax = std::max(nearMax, near);
    }
    auto c0 = std::chrono::steady_clock::now();
    bool dec = decide(d, f, rejoinSF);
    if (counting()) { cpuDecide += std::chrono::duration<double>(std::chrono::steady_clock::now() - c0).count(); nDecisions++; }
    if (dec) {
        // fairness option: every rule picks the rejoin SF from what network o measured, with the same rule
        if (rejoinSF <= 0 && par("commonRejoinSF").boolValue())
            rejoinSF = commonRejoinSF(d, f);
        trigger(devId, d, rejoinSF);
    }
}

bool Ecn::regress(const std::vector<std::pair<double,double>> &pts, double tStar, double sigma, double &pred, double &sPred, double &slope) const
{
    int n = pts.size();
    if (n < 2) return false;
    double mt = 0, my = 0;
    for (auto &p : pts) { mt += p.first; my += p.second; }
    mt /= n; my /= n;
    double sxx = 0, sxy = 0;
    for (auto &p : pts) { sxx += (p.first - mt) * (p.first - mt); sxy += (p.first - mt) * (p.second - my); }
    if (sxx <= 1e-9) return false;
    slope = sxy / sxx;
    pred = my + slope * (tStar - mt);
    sPred = sigma * std::sqrt(1.0 + 1.0 / n + (tStar - mt) * (tStar - mt) / sxx);
    return true;
}

int Ecn::commonRejoinSF(const EcnDevice &d, const EcnFrame &f) const
{
    // mean RSSI of network o over the last tauW seconds; lowest SF decoded with probability >= sfConfidence
    int s = f.serving, o = 1 - s;
    double sum = 0; int n = 0;
    for (auto &x : d.hist)
        if (x.t >= f.t - par("tauW").doubleValue() && x.serving == s && x.rx[o]) { sum += x.rssi[o]; n++; }
    if (n == 0) return par("dataSF").intValue();
    double mean = sum / n, sd = par("sigmaShadow").doubleValue() * std::sqrt(1.0 + 1.0 / n);
    for (int sf = 7; sf <= 12; sf++)
        if (phi((mean - sensitivity(sf) - par("reachMargin").doubleValue()) / sd) >= par("sfConfidence").doubleValue())
            return sf;
    return 12;
}

bool Ecn::decide(EcnDevice &d, const EcnFrame &f, int &rejoinSF)
{
    int s = f.serving, o = 1 - s;
    if (strategy == "naiveHear")
        return true;
    if (strategy == "naiveLose")
        return !f.rx[s] || f.rssi[s] < par("rssiThreshold").doubleValue();
    if (strategy == "hysteresis") {
        if (!f.rx[s] || f.rssi[o] - f.rssi[s] >= par("hystMargin").doubleValue()) d.hyst++;
        else d.hyst = 0;
        return d.hyst >= par("hystCount").intValue();
    }
    if (strategy == "hadec1") {
        // score of the first HADEC version (not used by default): Conf = Sn * (1 - exp(-k|H|)) * min(1, -delta/sigma)
        if (!f.rx[s]) return false;
        double prev = d.hasEma ? d.ema : f.rssi[s];
        double a = par("v1Alpha");
        d.ema = d.hasEma ? a * f.rssi[s] + (1 - a) * d.ema : f.rssi[s];
        d.hasEma = true;
        double delta = d.ema - prev;
        if (!(d.ema < par("v1TRssi").doubleValue() && delta < par("v1Gamma").doubleValue())) return false;
        auto clip = [](double v) { return std::max(0.0, std::min(1.0, v)); };
        double sn = par("v1W1").doubleValue() * clip((f.rssi[o] - par("v1RssiMin").doubleValue()) / (par("v1RssiMax").doubleValue() - par("v1RssiMin").doubleValue()))
                  + par("v1W2").doubleValue() * clip((f.snir[o] - par("v1SnrMin").doubleValue()) / (par("v1SnrMax").doubleValue() - par("v1SnrMin").doubleValue()));
        int h = 0;
        for (auto &x : d.hist) if (x.t >= f.t - par("v1History").doubleValue()) h++;
        double conf = sn * (1 - std::exp(-par("v1K").doubleValue() * h)) * clip(-delta / par("v1Sigma").doubleValue());
        return conf > par("v1Beta").doubleValue();
    }
    if (strategy == "hadec2") {
        // HADEC v2: probabilities computed from linear regressions over the last tauW seconds.
        // Frames missed by a network enter its regression as censored pseudo-observations S - offset.
        double tauW = par("tauW").doubleValue();
        double tMean = 60;
        if (d.hist.size() >= 2) tMean = (d.hist.back().t - d.hist.front().t) / (d.hist.size() - 1);
        double tStar = tMean;                                  // next action opportunity, relative to now
        double sig = par("sigmaShadow").doubleValue();
        double off = par("censorOffset").doubleValue();
        std::vector<std::pair<double,double>> dp, ap, bp;
        for (auto &x : d.hist) {
            if (x.t < f.t - tauW || x.serving != s) continue;
            double pa = x.rx[s] ? x.rssi[s] : sensitivity(x.sf) - off;
            double pb = x.rx[o] ? x.rssi[o] : sensitivity(x.sf) - off;
            if (x.rx[s] || x.rx[o]) { dp.push_back({x.t - f.t, pb - pa}); if (counting()) { nPts++; if (x.rx[s] != x.rx[o]) nCensored++; } }
            ap.push_back({x.t - f.t, pa});
            bp.push_back({x.t - f.t, pb});
        }
        double H = par("margin").doubleValue();
        double beta = par("beta"), gamma = par("gamma");
        double predD, sD, slD, predA, sA, slA, predB, sB, slB;
        int minPts = par("minPoints").intValue();
        bool okD = (int)dp.size() >= minPts && regress(dp, tStar, std::sqrt(2.0) * sig, predD, sD, slD);
        bool okA = (int)ap.size() >= 2 && regress(ap, tStar, sig, predA, sA, slA);
        bool okB = (int)bp.size() >= 2 && regress(bp, tStar, sig, predB, sB, slB);
        if (!okB) { predB = f.rssi[o]; sB = sig * std::sqrt(2.0); }
        // rejoin SF: lowest SF that B decodes with probability >= sfConfidence (data SF otherwise)
        int sfR = par("dataSF").intValue();
        if (par("adaptRejoinSF").boolValue()) {
            sfR = 12;
            for (int sf = 7; sf <= 12; sf++)
                if (phi((predB - sensitivity(sf) - par("reachMargin").doubleValue()) / sB) >= par("sfConfidence").doubleValue()) { sfR = sf; break; }
        }
        double mB = par("reachMargin").doubleValue();
        double pReachData = phi((predB - sensitivity(f.sf) - mB) / sB);   // B decodes the data frames
        double pBetter = okD ? phi((predD - H) / sD) : 0;                   // B better than A by H
        if (okD && counting()) preds[&d] = Pred{pBetter, H, s, (int)dp.size()};
        double pNotWorse = okD ? phi(predD / sD) : 0;
        bool goB = okD && slD > 0 && pBetter * pReachData >= beta;
        if (goB && par("dwellTest").boolValue()) {
            double p2, s2, sl2;
            regress(dp, tStar + par("dStar").doubleValue(), std::sqrt(2.0) * sig, p2, s2, sl2);
            goB = phi((p2 - H) / s2) >= beta;
        }
        // last chance: A is about to lose the device and B is at least as good
        double sens = sensitivity(f.sf) + par("lossMargin").doubleValue();
        bool lose = okA && phi((sens - predA) / sA) >= gamma && pNotWorse >= par("minReach").doubleValue();
        if (goB || lose) {
            if (counting()) { if (goB) triggerReasonB++; else triggerReasonLose++; }
            rejoinSF = sfR;
            return true;
        }
        return false;
    }
    if (strategy == "fall") {
        // adaptation of Fall et al. (GLOBECOM 2023): next-antenna prediction from past positions
        if (d.hist.size() < 2) return false;
        const EcnFrame &a = d.hist[d.hist.size() - 2], &b = d.hist.back();
        double sp = par("sigmaPos").doubleValue();
        double ax = a.x + normal(0, sp), ay = a.y + normal(0, sp), bx = b.x + normal(0, sp), by = b.y + normal(0, sp);
        double dt = std::max(1.0, b.t - a.t);
        double px = bx + (bx - ax) / dt * dt, py = by + (by - ay) / dt * dt;   // one interval ahead
        return bestNetAt(px, py) == o;
    }
    if (strategy == "oracle") {
        double tMean = 60;
        if (d.hist.size() >= 2) tMean = (d.hist.back().t - d.hist.front().t) / (d.hist.size() - 1);
        double step = par("oracleStep").doubleValue();
        double t0 = f.t + tMean, t1 = t0 + par("dStar").doubleValue();
        long devId = -1;
        for (auto &kv : devs) if (&kv.second == &d) { devId = kv.first; break; }
        for (double t = t0; t <= t1; t += step) {
            double x, y;
            if (!positionAt(devId, t, x, y)) return false;
            if (bestNetAt(x, y) != o) return false;
        }
        // B must decode the rejoin on average at the next opportunity (lowest such SF)
        double x0, y0;
        positionAt(devId, t0, x0, y0);
        double mb = meanRx(o, x0, y0);
        for (int sf = 7; sf <= 12; sf++)
            if (mb >= sensitivity(sf) + par("oracleMargin").doubleValue()) { rejoinSF = sf; return true; }
        return false;
    }
    throw cRuntimeError("unknown ECN strategy '%s'", strategy.c_str());
}

void Ecn::trigger(long devId, EcnDevice &d, int rejoinSF)
{
    auto m = new BackendMsg("Trigger");
    m->setBkType(BK_TRIGGER);
    m->setDevId(devId);
    m->setToNet(d.serving);
    m->setRejoinSF(rejoinSF > 0 ? rejoinSF : par("dataSF").intValue());
    m->setMaxRetries(par("maxRetries"));
    m->setRetryPeriod(par("retryPeriod").doubleValue());
    double hold = std::min(par("maxHold").doubleValue(), par("pendingHold").doubleValue() * std::pow(2.0, d.failures));
    d.pendingUntil = simTime().dbl() + hold;
    d.failures++;
    d.hyst = 0;
    if (counting()) { triggersSent++; d.triggers++; }
    cModule *target = getModuleByPath(serverPaths[d.serving].c_str());
    sendDirect(m, par("ecnDelay").doubleValue(), 0, target, "backendIn");
}

void Ecn::finish()
{
    if (calib) { fclose(calib); calib = nullptr; }
    recordScalar("ecnObs", obsReceived);
    recordScalar("ecnTriggers", triggersSent);
    recordScalar("ecnPrepares", preparesSent);
    recordScalar("ecnSwitchesSeen", switchesSeen);
    recordScalar("ecnTriggerReasonB", triggerReasonB);
    recordScalar("ecnTriggerReasonLose", triggerReasonLose);
    recordScalar("ecnDecisions", nDecisions);
    recordScalar("ecnDeltaPoints", nPts);
    recordScalar("ecnCensoredPoints", nCensored);
    recordScalar("ecnCpuDecide", cpuDecide);
    recordScalar("ecnNearMean", nearSamples ? nearSum / nearSamples : 0);
    recordScalar("ecnNearMax", nearMax);
    for (auto &kv : devs)
        recordScalar(("triggers " + std::to_string(kv.first)).c_str(), kv.second.triggers);
}

} // namespace flora
