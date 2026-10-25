//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "LoRaPhy/LoRaAnalogModel.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/wireless/common/analogmodel/scalar/ScalarSnir.h"
#include "inet/common/math/Functions.h"
#include "LoRaReception.h"
#include "LoRaReceptionAnalogModel.h"
#include "LoRaTransmission.h"
#include "LoRaTransmissionAnalogModel.h"
#include "LoRaReceiver.h"
#include "LoRa/LoRaRadio.h"

namespace flora {

Define_Module(LoRaAnalogModel);

std::ostream& LoRaAnalogModel::printToStream(std::ostream& stream, int level, int evFlags) const
{
    return stream << "LoRaAnalogModel";
}

const W LoRaAnalogModel::getBackgroundNoisePower(const LoRaBandListening *listening) const {
    //const LoRaBandListening *loRaListening = check_and_cast<const LoRaBandListening *>(listening);
    //Sensitivity values from Semtech SX1272/73 datasheet, table 10, Rev 3.1, March 2017
    W noisePower = W(math::dBmW2mW(-126.5) / 1000);
    if(listening->getLoRaSF() == 6)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-121) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-118) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-111) / 1000);
    }

    if (listening->getLoRaSF() == 7)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-124) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-122) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-116) / 1000);
    }

    if(listening->getLoRaSF() == 8)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-127) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-125) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-119) / 1000);
    }
    if(listening->getLoRaSF() == 9)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-130) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-128) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-122) / 1000);
    }
    if(listening->getLoRaSF() == 10)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-133) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-130) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-125) / 1000);
    }
    if(listening->getLoRaSF() == 11)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-135) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-132) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-128) / 1000);
    }
    if(listening->getLoRaSF() == 12)
    {
        if(listening->getLoRaBW() == Hz(125000)) noisePower = W(math::dBmW2mW(-137) / 1000);
        if(listening->getLoRaBW() == Hz(250000)) noisePower = W(math::dBmW2mW(-135) / 1000);
        if(listening->getLoRaBW() == Hz(500000)) noisePower = W(math::dBmW2mW(-129) / 1000);
    }
    return noisePower;
}

bool LoRaAnalogModel::areOverlappingBands(Hz centerFrequency1, Hz bandwidth1, Hz centerFrequency2, Hz bandwidth2) const
{
    return centerFrequency1 + bandwidth1 / 2 >= centerFrequency2 - bandwidth2 / 2 &&
           centerFrequency1 - bandwidth1 / 2 <= centerFrequency2 + bandwidth2 / 2;
}

W LoRaAnalogModel::computeReceptionPower(const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival) const
{
    const IRadioMedium *radioMedium = receiverRadio->getMedium();
    const INarrowbandSignalAnalogModel *narrowbandSignalAnalogModel = check_and_cast<const INarrowbandSignalAnalogModel *>(transmission->getAnalogModel());
    const IScalarSignalAnalogModel *scalarSignalAnalogModel = check_and_cast<const IScalarSignalAnalogModel *>(transmission->getAnalogModel());
    const Coord receptionStartPosition = arrival->getStartPosition();
    double transmitterAntennaGain = computeAntennaGain(transmission->getTransmitterAntennaGain(), transmission->getStartPosition(), arrival->getStartPosition(), transmission->getStartOrientation());
    double receiverAntennaGain = computeAntennaGain(receiverRadio->getAntenna()->getGain().get(), arrival->getStartPosition(), transmission->getStartPosition(), arrival->getStartOrientation());
    double pathLoss = radioMedium->getPathLoss()->computePathLoss(transmission, arrival);
    double obstacleLoss = radioMedium->getObstacleLoss() ? radioMedium->getObstacleLoss()->computeObstacleLoss(narrowbandSignalAnalogModel->getCenterFrequency(), transmission->getStartPosition(), receptionStartPosition) : 1;
    W transmissionPower = scalarSignalAnalogModel->getPower();
    return transmissionPower * std::min(1.0, transmitterAntennaGain * receiverAntennaGain * pathLoss * obstacleLoss);
}

const IReception *LoRaAnalogModel::computeReception(const IRadio *receiverRadio, const ITransmission *transmission, const IArrival *arrival) const
{
    const LoRaTransmission *loRaTransmission = check_and_cast<const LoRaTransmission *>(transmission);
    const simtime_t receptionStartTime = arrival->getStartTime();
    const simtime_t receptionEndTime = arrival->getEndTime();
    const Quaternion receptionStartOrientation = arrival->getStartOrientation();
    const Quaternion receptionEndOrientation = arrival->getEndOrientation();
    const Coord receptionStartPosition = arrival->getStartPosition();
    const Coord receptionEndPosition = arrival->getEndPosition();
    W receivedPower = computeReceptionPower(receiverRadio, transmission, arrival);
    Hz LoRaCF = loRaTransmission->getLoRaCF();
    int LoRaSF = loRaTransmission->getLoRaSF();
    Hz LoRaBW = loRaTransmission->getLoRaBW();
    int LoRaCR = loRaTransmission->getLoRaCR();
    return new LoRaReception(receiverRadio, transmission, receptionStartTime, receptionEndTime, receptionStartPosition, receptionEndPosition, receptionStartOrientation, receptionEndOrientation, LoRaCF, LoRaBW, receivedPower, LoRaSF, LoRaCR);
}

const INoise *LoRaAnalogModel::computeNoise(const IListening *listening, const IInterference *interference) const
{
    const LoRaBandListening *bandListening = check_and_cast<const LoRaBandListening *>(listening);
    Hz commonCarrierFrequency = bandListening->getLoRaCF();
    Hz commonBandwidth = bandListening->getLoRaBW();
    simtime_t noiseStartTime = SimTime::getMaxTime();
    simtime_t noiseEndTime = 0;
    std::map<simtime_t, W> powerChanges;
    const std::vector<const IReception *> *interferingReceptions = interference->getInterferingReceptions();
    for (auto reception : *interferingReceptions) {
        const IReceptionAnalogModel *signalAnalogModel = reception->getAnalogModel();
        const INarrowbandSignalAnalogModel *narrowbandSignalAnalogModel = check_and_cast<const INarrowbandSignalAnalogModel *>(signalAnalogModel);
        const LoRaReceptionAnalogModel *loRaReceptionAnalogModel = check_and_cast<const LoRaReceptionAnalogModel *>(signalAnalogModel);
        Hz signalCarrierFrequency = loRaReceptionAnalogModel->getLoRaCF();
        Hz signalBandwidth = loRaReceptionAnalogModel->getLoRaBW();
        if((commonCarrierFrequency == signalCarrierFrequency && commonBandwidth == signalBandwidth))
        {
            const IScalarSignalAnalogModel *scalarSignalAnalogModel = check_and_cast<const IScalarSignalAnalogModel *>(signalAnalogModel);
            W power = scalarSignalAnalogModel->getPower();
            simtime_t startTime = reception->getStartTime();
            simtime_t endTime = reception->getEndTime();
            if (startTime < noiseStartTime)
                noiseStartTime = startTime;
            if (endTime > noiseEndTime)
                noiseEndTime = endTime;
            std::map<simtime_t, W>::iterator itStartTime = powerChanges.find(startTime);
            if (itStartTime != powerChanges.end())
                itStartTime->second += power;
            else
                powerChanges.insert(std::pair<simtime_t, W>(startTime, power));
            std::map<simtime_t, W>::iterator itEndTime = powerChanges.find(endTime);
            if (itEndTime != powerChanges.end())
                itEndTime->second -= power;
            else
                powerChanges.insert(std::pair<simtime_t, W>(endTime, -power));
        }
        else if (areOverlappingBands(commonCarrierFrequency, commonBandwidth, narrowbandSignalAnalogModel->getCenterFrequency(), narrowbandSignalAnalogModel->getBandwidth()))
            throw cRuntimeError("Overlapping bands are not supported");
    }

    simtime_t startTime = listening->getStartTime();
    simtime_t endTime = listening->getEndTime();
    const W noisePower = getBackgroundNoisePower(bandListening);
    auto jt1 = powerChanges.find(startTime);
    if (jt1 != powerChanges.end())
        jt1->second += noisePower;
    else
        powerChanges.insert(std::pair<simtime_t, W>(startTime, noisePower));
    auto jt2 = powerChanges.find(endTime);
    if (jt2 != powerChanges.end())
        jt2->second -= noisePower;
    else
        powerChanges.insert(std::pair<simtime_t, W>(endTime, -noisePower));

    powerChanges[inet::math::getLowerBound<simtime_t>()] = W(0);
    powerChanges[inet::math::getUpperBound<simtime_t>()] = W(0);
    EV_TRACE << "Noise power begin " << endl;
    W noise = W(0);
    for (auto it = powerChanges.begin(); it != powerChanges.end(); it++) {
        noise += it->second;
        it->second = noise;
        EV_TRACE << "Noise at " << it->first << " = " << noise << endl;
    }
    EV_TRACE << "Noise power end" << endl;
    const auto& powerFunction = inet::makeShared<inet::math::Interpolated1DFunction<W, simtime_t>>(powerChanges, &inet::math::LeftInterpolator<simtime_t, W>::singleton);
    return new ScalarNoise(noiseStartTime, noiseEndTime, commonCarrierFrequency, commonBandwidth, powerFunction);
}

const INoise *LoRaAnalogModel::computeNoise(const IReception *reception, const INoise *noise) const
{
    auto scalarNoise = check_and_cast<const ScalarNoise *>(noise);
    auto receptionAnalogModel = check_and_cast<const LoRaReceptionAnalogModel *>(reception->getAnalogModel());
    simtime_t noiseStartTime = SimTime::getMaxTime();
    simtime_t noiseEndTime = 0;
    std::map<simtime_t, W> powerChanges;
    powerChanges[math::getLowerBound<simtime_t>()] = W(0);
    powerChanges[math::getUpperBound<simtime_t>()] = W(0);
    // add reception power
    W power = receptionAnalogModel->getPower();
    simtime_t convergenceStartTime = reception->getStartTime();
    simtime_t convergenceEndTime = reception->getEndTime();
    powerChanges[convergenceStartTime] += power;
    powerChanges[convergenceEndTime] -= power;
    if (convergenceStartTime < noiseStartTime) noiseStartTime = convergenceStartTime;
    if (convergenceEndTime > noiseEndTime) noiseEndTime = convergenceEndTime;
    // add background noise
    const auto& noisePowerFunction = scalarNoise->getPower();
    math::Point<simtime_t> startPoint(scalarNoise->getStartTime());
    math::Point<simtime_t> endPoint(scalarNoise->getEndTime());
    math::Interval<simtime_t> interval(startPoint, endPoint, 0b1, 0b0, 0b0);
    noisePowerFunction->partition(interval, [&] (const math::Interval<simtime_t>& i1, const math::IFunction<W, math::Domain<simtime_t>> *f) {
        auto lower = std::get<0>(i1.getLower());
        auto upper = std::get<0>(i1.getUpper());
        auto fc = check_and_cast<const math::ConstantFunction<W, math::Domain<simtime_t>> *>(f);
        powerChanges[lower] += fc->getConstantValue();
        powerChanges[upper] -= fc->getConstantValue();
    });
    if (scalarNoise->getStartTime() < noiseStartTime) noiseStartTime = scalarNoise->getStartTime();
    if (scalarNoise->getEndTime() > noiseEndTime) noiseEndTime = scalarNoise->getEndTime();
    W noisePower = W(0);
    for (auto & it : powerChanges) {
        noisePower += it.second;
        it.second = noisePower;
    }
    const auto& resultPowerFunction = makeShared<math::Interpolated1DFunction<W, simtime_t>>(powerChanges, &math::LeftInterpolator<simtime_t, W>::singleton);
    return new ScalarNoise(noiseStartTime, noiseEndTime, scalarNoise->getCenterFrequency(), scalarNoise->getBandwidth(), resultPowerFunction);
}

const ISnir *LoRaAnalogModel::computeSNIR(const IReception *reception, const INoise *noise) const
{
    return new ScalarSnir(reception, noise);
}

} // namespace inet

