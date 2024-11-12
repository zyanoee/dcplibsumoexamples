
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

#include <libsumo/libsumo.h>
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

    std::vector<std::string> edgeIDs;
    std::string old_id = "";





public:
    Slave() : stdLog(std::cout) {
        udpDriver = new UdpDriver(HOST, PORT);
        std::cout << "Gestione dello SlaveDescription" << std::endl;
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
        std::cout << "Inizio configurazione" << std::endl;
        simulationTime = 0;
        currentStep = 0;

        sem_value = manager->getInput<uint8_t *>(sem_vr);
        pos = manager->getOutput<char*>(pos_vr);
        posStr = new DcpString(pos);


        std::cout << "Inizio connessione a SUMO" << std::endl;
        //CONNESSIONE A SUMO ATTRAVERSO TRACI
        libsumo::Simulation::start({"sumo", "-c", "../sumo-network-example/config.sumocfg"});
        libsumo::Simulation::step(1);

        std::cout << "Connessione a SUMO riuscita" << std::endl;
        //OTTENIAMO GLI EDGE DELLA RETE
        std::cout << "Ottenimento topologia mappa" << std::endl;
        edgeIDs = libsumo::Edge::getIDList();
        std::cout << "Done!" << std::endl;
    }

    void initialize() {
        std::string str("");
        posStr->setString(str);
    }

    void doStep(uint64_t steps) {
        float64_t timeDiff =
                ((double) numerator) / ((double) denominator) * ((double) steps);

        //STEP SUMO
        std::cout<<"Inizio step SUMO"<<std::endl;
        libsumo::Simulation::step(1);

        //OTTENIAMO I VEICOLI PRESENTI IN SIMULAZIONE
        std::vector<std::string> vehicleIDs = libsumo::Vehicle::getIDList();

        //ITERIAMO SUI VEICOLI E OTTENIAMO LE POSIZIONI
        //per semplicità e per esempio della classe DcpString userò una stringa
        //del tipo id#posx#posy@id2#posx2#posy2 ...
        std::string outputString = "";
        for (const auto& id : vehicleIDs) {
                std::cout<<"Iterazione sul veicolo con ID: "<< id <<std::endl;
                double posx = libsumo::Vehicle::getPosition(id, false).x;
                double posy = libsumo::Vehicle::getPosition(id, false).y;
                std::string tmp = id + "#" + std::to_string(posx) + "#" + std::to_string(posy) + "@";
                outputString += tmp;
        }

        posStr -> setString(outputString);

        if(*sem_value >= 128){
                std::cout << "Cambio edge del network" << std::endl;
                int random_index = *sem_value%edgeIDs.size();
                std::string newRand = edgeIDs[random_index];
                std::cout << "Edge con id " << newRand << "Ora inaccessibile, Ripristino dell'edge con id "<<old_id << std::endl;
                libsumo::Edge::setMaxSpeed(newRand, 0.0);
                //RIPRISTINO VECCHIO EDGE FERMO
                if(old_id!=""){libsumo::Edge::setMaxSpeed(old_id, 30.0);}
                old_id=newRand;
                //REROUTING DATO EDGE FERMO
                for (const auto& id : vehicleIDs) {
                        libsumo::Vehicle::rerouteTraveltime(id);
                }

        }

        std::cout << (SIM_LOG, simulationTime, currentStep) << std::endl;
        simulationTime += timeDiff;
        currentStep += steps;
    }

    void setTimeRes(const uint32_t numerator, const uint32_t denominator) {
        this->numerator = numerator;
        this->denominator = denominator;
    }

    void start() {
    std::cout << "Avvio del Manager" << std::endl;
    manager->start();}

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

        std::cout << "--Gestione dei pointer per gli output" << std::endl;
        std::shared_ptr<Output_t> caus_pos = make_Output_String_ptr();
		caus_pos->String->maxSize = std::make_shared<uint32_t>(9999);
		caus_pos->String->start = std::make_shared<std::string>(" ");
        slaveDescription.Variables.push_back(make_Variable_output("pos", pos_vr, caus_pos));
        std::cout << "--Gestione dei pointer per gli input" << std::endl;
        std::shared_ptr<CommonCausality_t> caus_a =
                make_CommonCausality_ptr<uint8_t>();
        caus_a->Uint8->start = std::make_shared<std::vector<uint8_t>>();
        caus_a->Uint8->start->push_back(0);
        slaveDescription.Variables.push_back(make_Variable_input("sem_value", sem_vr, caus_a));
        slaveDescription.Log = make_Log_ptr();
        slaveDescription.Log->categories.push_back(make_Category(1, "DCP_SLAVE"));
        std::cout << "--SlaveDescription creato" << std::endl;

       return slaveDescription;
    }

};

#endif
