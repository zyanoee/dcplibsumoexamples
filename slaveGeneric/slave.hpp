#ifndef SLAVE_H_
#define SLAVE_H_

#include <dcp/helper/Helper.hpp>
#include <dcp/log/OstreamLog.hpp>
#include <dcp/logic/DcpManagerSlave.hpp>
#include <dcp/model/pdu/DcpPduFactory.hpp>
#include <dcp/driver/ethernet/udp/UdpDriver.hpp>
#include <dcp/xml/DcpSlaveDescriptionWriter.hpp>

#include <cstdint>
#include <cstdio>
#include <stdarg.h>
#include <thread>
#include <cmath>
#include <random>
#include <iostream>

class Slave {
public:
    Slave() : stdLog(std::cout) {
        rng.seed(rd());
        udpDriver = new UdpDriver(HOST, PORT);
        std::cout << "Gestione dello SlaveDescription" << std::endl;
        SlaveDescription_t slaved = getSlaveDescription();
        manager = new DcpManagerSlave(slaved, udpDriver->getDcpDriver());
        manager->setInitializeCallback<SYNC>(
            std::bind(&Slave::initialize, this));
        manager->setConfigureCallback<SYNC>(
            std::bind(&Slave::configure, this));
        manager->setSynchronizingStepCallback<SYNC>(
            std::bind(&Slave::doStep, this, std::placeholders::_1));
        manager->setSynchronizedStepCallback<SYNC>(
            std::bind(&Slave::doStep, this, std::placeholders::_1));
        manager->setRunningStepCallback<SYNC>(
            std::bind(&Slave::doStep, this, std::placeholders::_1));
        manager->setSynchronizedNRTStepCallback<SYNC>(
            std::bind(&Slave::doStep, this, std::placeholders::_1));
        manager->setRunningNRTStepCallback<SYNC>(
            std::bind(&Slave::doStep, this, std::placeholders::_1));
        manager->setTimeResListener<SYNC>(std::bind(&Slave::setTimeRes, this,
                                                    std::placeholders::_1,
                                                    std::placeholders::_2));

        //Display log messages on console
        manager->addLogListener(
            std::bind(&OstreamLog::logOstream, stdLog, std::placeholders::_1));
        manager->setGenerateLogString(true);
        std::cout << "Creazione del file xml" << std::endl;
        writeDcpSlaveDescription(slaved, "randomRNGSlave.xml");
    }

    ~Slave() {
        delete manager;
        delete udpDriver;
    }


    void configure() {
        std::cout << "Configurazione..." << std::endl;
        simulationTime = 0;
        currentStep = 0;

        a = manager->getOutput<uint8_t *>(a_vr);
        *a = 0;
    }

    void initialize() {
        std::cout << "Inizializzazione..." << std::endl;
        *a = 0;
    }

    void doStep(uint64_t steps) {
        std::cout << "Chiamata del doStep" << std::endl;
        float64_t timeDiff =
            ((double)numerator) / ((double)denominator) * (currentStep);

        //Funzione che restituisce un valore pseudocasuale che differisce progressivamente
        //dal numero iniziale e ricomincia la sua logica dopo che la distanza dal numero si fa
        //molto grande

        std::uniform_int_distribution<int> dist(1, current_distance);
        std::cout << "Distribuzione casuale" << std::endl;
        current_value += dist(rng);
        std::cout << "current value modificato" << std::endl;
        if (current_distance < max_distance) {
            ++current_distance;
        } else {
            current_distance = 1;
        }
        *a=current_value;
        std::cout << "[ "<< timeDiff <<" ]" << "Nuovo valore pseudorandomico: "<< std::to_string(*a) << std::endl;
        simulationTime += timeDiff;
        currentStep += steps;
    }

    void setTimeRes(const uint32_t numerator, const uint32_t denominator) {
        this->numerator = numerator;
        this->denominator = denominator;
    }

    void start() { manager->start(); }

    SlaveDescription_t getSlaveDescription(){
        std::cout << "--Creazione puntatore" << std::endl;
        SlaveDescription_t slaveDescription = make_SlaveDescription(1, 0, "salveRNGGen", "2fcef2a4-51d0-11ec-bf63-0242ac130002");
        std::cout << "--Impostazione modalità operativa: NRT" << std::endl;
        slaveDescription.OpMode.NonRealTime = make_NonRealTime_ptr();
        Resolution_t resolution = make_Resolution();
        resolution.numerator = 1;
        resolution.denominator = 100;
        slaveDescription.TimeRes.resolutions.push_back(resolution);
        std::cout << "--Impostazione risoluzione temporale 1/100" << std::endl;
        slaveDescription.TransportProtocols.UDP_IPv4 = make_UDP_ptr();
        slaveDescription.TransportProtocols.UDP_IPv4->Control =
            make_Control_ptr(HOST, 8082);
        ;
        slaveDescription.TransportProtocols.UDP_IPv4->DAT_input_output = make_DAT_ptr();
        slaveDescription.TransportProtocols.UDP_IPv4->DAT_input_output->availablePortRanges.push_back(
            make_AvailablePortRange(2048, 65535));
        slaveDescription.TransportProtocols.UDP_IPv4->DAT_parameter = make_DAT_ptr();
        slaveDescription.TransportProtocols.UDP_IPv4->DAT_parameter->availablePortRanges.push_back(
            make_AvailablePortRange(2048, 65535));
        slaveDescription.CapabilityFlags.canAcceptConfigPdus = true;
        slaveDescription.CapabilityFlags.canHandleReset = true;
        slaveDescription.CapabilityFlags.canHandleVariableSteps = true;
        slaveDescription.CapabilityFlags.canMonitorHeartbeat = false;
        slaveDescription.CapabilityFlags.canAcceptConfigPdus = true;
        slaveDescription.CapabilityFlags.canProvideLogOnRequest = true;
        slaveDescription.CapabilityFlags.canProvideLogOnNotification = true;
        std::cout << "--Gestione I/O" << std::endl;
        std::shared_ptr<Output_t> caus_y = make_Output_ptr<uint8_t>();
        std::cout << "--A" << std::endl;
        slaveDescription.Variables.push_back(make_Variable_output("sem_val", a_vr, caus_y));
        std::cout << "--C" << std::endl;
        slaveDescription.Log = make_Log_ptr();
        std::cout << "--D" << std::endl;
        slaveDescription.Log->categories.push_back(make_Category(1, "DCP_SLAVE"));
        std::cout << "--E" << std::endl;
        slaveDescription.Log->templates.push_back(make_Template(
            1, 1, (uint8_t)DcpLogLevel::LVL_INFORMATION,  "[Time = %float64]: Random value is: %uint8"));
        std::cout << "--Done!" << std::endl;
        return slaveDescription;
    }

private:
    DcpManagerSlave *manager;
    OstreamLog stdLog;

    UdpDriver* udpDriver;
    const char *const HOST = "127.0.0.1";
    const int PORT = 8082;

    uint32_t numerator;
    uint32_t denominator;

    double simulationTime;
    uint64_t currentStep;

    const LogTemplate SIM_LOG = LogTemplate(
        1, 1, DcpLogLevel::LVL_INFORMATION,
        "[Time = %float64]: Random value is: %uint8",
        {DcpDataType::float64, DcpDataType::uint8});

    uint8_t *a;
    const uint32_t a_vr = 2;


    int current_value=2;
    int max_distance=20;
    int current_distance=1;
    std::random_device rd;
    std::mt19937 rng;

};

#endif /* SLAVE_H_ */
