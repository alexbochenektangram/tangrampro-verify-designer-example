#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "Serializer.hpp"
#include "TangramTransportTypes.h"
#include "afrl_cmasi_DerivedEntityFactory.hpp"
#include "TangramTransport.hpp"
#include "LMCPSerializer.hpp"

#include "afrl/cmasi/AirVehicleState.hpp"
#include "afrl/cmasi/CameraAction.hpp"
#include "afrl/cmasi/CameraConfiguration.hpp"
#include "afrl/cmasi/CameraState.hpp"
#include "afrl/cmasi/GoToWaypointAction.hpp"
#include "afrl/cmasi/MissionCommand.hpp"

using namespace tangram;
using namespace genericapi;
using namespace serializers;
using namespace transport;
using namespace std::chrono_literals;

bool sendMessage(Message& m, std::shared_ptr<TangramTransport> tport, LMCPSerializer& ser) {
    static std::vector<uint8_t> buffer;

    buffer.clear();

    if (!ser.serialize(m, buffer)) {
        std::cerr << "Failed to serialize message " << m.getName() << std::endl;
        return false;
    }

    std::string topic = "afrl.cmasi." + m.getName();

    if (!tport->publish(buffer.data(), buffer.size(), topic)) {
        std::cerr << "Failed to publish message " << m.getName() << std::endl;
        return false;
    }

    return true;
}

bool recvMessage(
    Message& msg,
    std::shared_ptr<TangramTransport> tport,
    LMCPSerializer& ser
) {
    static std::vector<uint8_t> buffer;

    buffer.resize(tport->getMaxReceiveSize());
    int32_t count = tport->recv(buffer.data(), buffer.size());
    if (count < 0) {
        std::cerr << "Failed to receive bytes for " << msg.getName() << std::endl;
        return false;
    }
    buffer.resize(count);
    std::cout << "Received bytes for " << msg.getName() << std::endl;

    if (ser.deserialize(buffer, msg)) {
        std::cout << "Deserialized " << msg.getName() << std::endl;
        return true;
    } else {
        std::cerr << "Failed to deser " << msg.getName() << std::endl;
    }

    return false;
}

uint8_t recvEitherMessage(
    Message& msg1,
    Message& msg2,
    std::shared_ptr<TangramTransport> tport,
    LMCPSerializer& ser
) {
    static std::vector<uint8_t> buffer;

    buffer.resize(tport->getMaxReceiveSize());
    int32_t count = tport->recv(buffer.data(), buffer.size());
    if (count < 0) {
        std::cerr << "Failed to receive bytes for " << msg1.getName() << " or " << msg2.getName() << std::endl;
        return false;
    }
    buffer.resize(count);

    if (ser.deserialize(buffer, msg1)) {
        std::cout << "Deserialized " << msg1.getName() << std::endl;
        return 1;
    }
    if (ser.deserialize(buffer, msg2)) {
        std::cout << "Deserialized " << msg2.getName() << std::endl;
        return 2;
    }

    return 0;
}

int main(int argc, char **argv) {
    // Collect args
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }

    // First try to set configuration from environment variables. Then, try to set from args
    // Args should override env
    std::string ip = "127.0.0.1";
    char* maybe_value = std::getenv("TANGRAM_TRANSPORT_zeromq_transport_HOSTNAME");
    if (maybe_value != nullptr) {
        ip = maybe_value;
    }
    if (args.size() > 1) {
        ip = args[1];
    }

    std::string pub_port = "6667";
    std::string sub_port = "6668";
    // Expected to be of form 6667,6668 (pub_port,sub_port)

    maybe_value = std::getenv("TANGRAM_TRANSPORT_zeromq_transport_PORTS");
    if (maybe_value != nullptr) {
        std::string ports(maybe_value);

        // split the ports at the comma
        auto comma_pos = ports.find(",");
        if (comma_pos == std::string::npos) {
            std::cerr << "Unexpected lack of comma in PORTS env variable" << std::endl;
        } else {
            // pub port, then sub port
            pub_port = ports.substr(0, comma_pos);
            sub_port = ports.substr(comma_pos + 1);
        }
    }
    if (args.size() > 2) {
        sub_port = args[2];
    }
    if (args.size() > 3) {
        pub_port = args[3];
    }

    // Configure the factory & serializer
    afrl::cmasi::DerivedEntityFactory factory;
    LMCPSerializer serializer(&factory);

    // Configure the transport
    std::shared_ptr<TangramTransport> tx = std::shared_ptr<TangramTransport>(TangramTransport::createTransport());
    std::shared_ptr<TangramTransport> rx = std::shared_ptr<TangramTransport>(TangramTransport::createTransport());
    if (tx == nullptr || rx == nullptr) {
        std::cerr << "Failed to create transport" << std::endl;
        exit(1);
    }

    rx->setOption("SubscribeIP", ip);
    rx->setOption("SubscribePort", sub_port);
    tx->setOption("PublishIP", ip);
    tx->setOption("PublishPort", pub_port);

    if (-1 == tx->open(TTF_WRITE)) {
        std::cerr << "Failed to open tx transport" << std::endl;
        return 1;
    }
    std::cout << "Opened tx transport" << std::endl;
    if (-1 == rx->open(TTF_READ)) {
        std::cerr << "Failed to open rx transport" << std::endl;
        return 1;
    }
    std::cout << "Opened rx transport" << std::endl;

    // Subscribe to the topic that the message will come in on
    rx->subscribe("afrl.cmasi.MissionCommand");
    rx->subscribe("afrl.cmasi.GoToWaypointAction");
    rx->subscribe("afrl.cmasi.CameraAction");

    // Give the transport time to initialize & connect to the proxy
    std::this_thread::sleep_for(10ms);

    // Message handling
    std::cout << "Waiting for first message..." << std::endl;

    afrl::cmasi::MissionCommand mc;
    afrl::cmasi::CameraAction ca;
    auto msgid = recvEitherMessage(mc, ca, rx, serializer);
    if (msgid == 1) {
        std::cout << "Received MissionCommand" << std::endl;

        afrl::cmasi::AirVehicleState avs1;
        if (!sendMessage(avs1, tx, serializer)) {
            std::cerr << "Failed to send first AirVehicleState" << std::endl;
            return 1;
        }
        std::cout << "Sent first AirVehicleState" << std::endl;

        afrl::cmasi::GoToWaypointAction gtw;
        recvMessage(gtw, rx, serializer);
        std::cout << "Received GoToWaypointAction" << std::endl;

        if (gtw.getWaypointNumber() != mc.getWaypointList()[0]->getNumber()) {
            std::cerr << "Mismatch in MissionCommand <> GoToWaypointAction Waypoint Number" << std::endl;
            return 1;
        }

        afrl::cmasi::AirVehicleState avs2;
        afrl::cmasi::Location3D loc;
        loc.setLatitude(mc.getWaypointList()[0]->getLatitude());
        loc.setLongitude(mc.getWaypointList()[0]->getLongitude());
        avs2.setLocation(&loc);
        if (!sendMessage(avs2, tx, serializer)) {
            std::cerr << "Failed to send second AirVehicleState" << std::endl;
            return 1;
        }
        std::cout << "Sent second AirVehicleState" << std::endl;
    } else if (msgid == 2) {
        std::cout << "Received first CameraAction" << std::endl;

        afrl::cmasi::CameraConfiguration cfg;
        cfg.setMinHorizontalFieldOfView(20);
        cfg.setMaxHorizontalFieldOfView(150);
        if (!sendMessage(cfg, tx, serializer)) {
            std::cerr << "Failed to send CameraConfiguration" << std::endl;
            return 1;
        }
        std::cout << "Sent CameraConfiguration" << std::endl;

        afrl::cmasi::CameraAction act;
        recvMessage(act, rx, serializer);
        std::cout << "Received second CameraAction" << std::endl;

        auto fov = act.getHorizontalFieldOfView();
        if (fov < 20 || fov > 150) {
            std::cerr << "CameraAction request out of bounds" << std::endl;
            return 1;
        }

        afrl::cmasi::CameraState state;
        state.setHorizontalFieldOfView(fov);
        if (!sendMessage(state, tx, serializer)) {
            std::cerr << "Failed to send CameraState" << std::endl;
            return 1;
        }
        std::cout << "Sent CameraState" << std::endl;
    } else {
        std::cerr << "Failed to receive a proper message to start any sequence" << std::endl;
        return 1;
    }

    return 0;
}
