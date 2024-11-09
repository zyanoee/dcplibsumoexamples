
#ifndef SLAVE_H_
#define SLAVE_H_

#include <dcp/helper/Helper.hpp>
#include <dcp/log/OstreamLog.hpp>
#include <dcp/logic/DcpManagerSlave.hpp>
#include <dcp/model/pdu/DcpPduFactory.hpp>
#include <dcp/driver/ethernet/udp/UdpDriver.hpp>

#include <cstdint>
#include <cstdio>
#include <stdarg.h>
#include <thread>
#include <cmath>
#include <iostream>

#include <libsumo/libtraci.h>

class Slave {
private:
    DcpManagerSlave *manager;
    OstreamLog stdLog;

    UdpDriver* udpDriver;
    const char *const HOST = "127.0.0.1";
    const int PORT = 8080;

    const char *const sumoHost = "127.0.0.1";
    const int sumoPort = 3377;

    uint32_t numerator;
    uint32_t denominator;

    double simulationTime;
    uint64_t currentStep;

    const LogTemplate SIM_LOG = LogTemplate(
            1, 1, DcpLogLevel::LVL_INFORMATION,
            "[Time = %float64]: sin(%uint64 + %float64) = %float64",
            {DcpDataType::float64, DcpDataType::uint64, DcpDataType::float64, DcpDataType::float64});

    char* pos;
    DcpString* posStr;
    const uint32_t pos_vr = 1;

    uint8_t* sem_value;
    const uint32_t sem_vr = 2;





}

public:
    Slave() : stdLog(std::cout) {
        udpDriver = new UdpDriver(HOST, PORT);
        manager = new DcpManagerSlave(getSlaveDescription(), udpDriver->getDcpDriver());
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
        manager->setTimeResListener<SYNC>(std::bind(&Slave::setTimeRes, this,
                                                    std::placeholders::_1,
                                                    std::placeholders::_2));

        //Display log messages on console
        manager->addLogListener(
                std::bind(&OstreamLog::logOstream, stdLog, std::placeholders::_1));
        manager->setGenerateLogString(true);

    }

    ~Slave() {
        delete manager;
        delete udpDriver;
    }


    void configure() {
        simulationTime = 0;
        currentStep = 0;

        sem_value = manager->getInput<uint8_t *>(sem_vr);
        pos = manager->getOutput<char*>(pos_vr);
        posStr = new DcpString(pos);

         try {
            traci.connect(sumoHost, sumoPort);
            std::cout << "Connesso a SUMO su " << sumoHost << ":" << sumoPort << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Errore di connessione a SUMO: " << e.what() << std::endl;
            return false;
        }
    }

    void initialize() {
        posStr->setString(" ");
    }

    void doStep(uint64_t steps) {
        float64_t timeDiff =
                ((double) numerator) / ((double) denominator) * ((double) steps);


        *y = std::sin(currentStep + *a);

        manager->Log(SIM_LOG, simulationTime, currentStep, *a, *y);
        simulationTime += timeDiff;
        currentStep += steps;
    }

    void setTimeRes(const uint32_t numerator, const uint32_t denominator) {
        this->numerator = numerator;
        this->denominator = denominator;
    }

    void start() { manager->start(); }

    SlaveDescription_t getSlaveDescription(){
        SlaveDescription_t slaveDescription = make_SlaveDescription(1, 0, "slaveSumo", "b5279485-720d-4542-9f29-bee4d9a75000");
        slaveDescription.OpMode.SoftRealTime = make_SoftRealTime_ptr();
        Resolution_t resolution = make_Resolution();
        resolution.numerator = 1;
        resolution.denominator = 100;
        slaveDescription.TimeRes.resolutions.push_back(resolution);
        slaveDescription.TransportProtocols.UDP_IPv4 = make_UDP_ptr();
        slaveDescription.TransportProtocols.UDP_IPv4->Control =
                make_Control_ptr(HOST, 8080);
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

        std::shared_ptr<Output_t> caus_pos = make_Output_String_ptr();
		caus_y->String->maxSize = std::make_shared<uint32_t>(2000);
		caus_y->String->start = std::make_shared<std::string>(" ");
        slaveDescription.Variables.push_back(make_Variable_output("pos", pos_vr, caus_pos));
        std::shared_ptr<CommonCausality_t> caus_a =
                make_CommonCausality_ptr<float64_t>();
        caus_a->Uint8->start = std::make_shared<std::vector<float64_t>>();
        caus_a->Uint8->start->push_back(0);
        slaveDescription.Variables.push_back(make_Variable_input("a", a_vr, caus_a));
        slaveDescription.Log = make_Log_ptr();
        slaveDescription.Log->categories.push_back(make_Category(1, "DCP_SLAVE"));
        slaveDescription.Log->templates.push_back(make_Template(
                1, 1, (uint8_t) DcpLogLevel::LVL_INFORMATION, "[Time = %float64]: sin(%uint64 + %float64) = %float64"));

       return slaveDescription;
    }

};

#endif
