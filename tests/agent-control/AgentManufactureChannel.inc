std::string token(unsigned number)
{
    std::ostringstream value; value << std::hex << std::setw(32) << std::setfill('0') << number;
    return value.str();
}
struct TableChannel {
    AgentTableControllerState state, unsupportedParent;
    AgentTableControllerGame game;
    std::unique_ptr<AgentTableTestController> controller;
    int peer = -1;
    std::string epoch, last, request, prefix, pending;
    unsigned challenge = 10, command = 100;
    std::vector<std::string> schedule;
    TableChannel(const std::string &name) : prefix(name)
    {
        int descriptors[2]; check(!socketpair(AF_UNIX, SOCK_SEQPACKET, 0, descriptors), "actual anonymous controller socket");
        peer = descriptors[1]; setenv("OPENXCOM_AGENT_CONTROL", "1", 1);
        setenv("OPENXCOM_AGENT_CONTROL_FD", std::to_string(descriptors[0]).c_str(), 1); unsetenv("OPENXCOM_STANCE_FD");
        game.top = &state; unsupportedParent.project = [](AgentUiState &) { return false; };
        controller.reset(new AgentTableTestController(&game));
        const std::string hello = receive(); check(!hello.empty(), "actual controller hello");
        epoch = YAML::Load(hello)["epoch"].as<std::string>();
        send(std::string("H ") + agentProtocolSchema()); controller->poll();
        check(!receive().empty(), "actual controller ready handshake");
    }
    ~TableChannel()
    {
        controller->close(); if (peer >= 0) ::close(peer);
        std::string json = "["; for (const auto &entry : schedule) { if (json.size() > 1) json += ","; json += entry; }
        save(prefix + "-schedule.json", json + "]");
    }
    void save(const std::string &name, const std::string &value)
    {
        const char *directory = std::getenv("OPENXCOM_AGENT_TEST_PACKET_DIR");
        if (directory) { std::ofstream stream(std::string(directory) + "/" + name); stream << value; check(bool(stream), "public packet persisted"); }
    }
    void send(const std::string &value)
    { pending = value; check(::send(peer, value.data(), value.size(), 0) == ssize_t(value.size()), "send actual controller request"); }
    std::string receive()
    {
        char buffer[65536]; const auto length = ::recv(peer, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (length <= 0) return {};
        const std::string value(buffer, length), name = prefix + "-wire-" + std::to_string(schedule.size()) + ".json";
        save(name, value);
        schedule.push_back("{\"request\":" + (pending.empty() ? std::string("null") : agentJsonString(pending)) +
            ",\"response_file\":" + agentJsonString(name) + "}");
        pending.clear(); return value;
    }
    void frame() { controller->beginFrame(); controller->presented(); }
    std::string read()
    {
        send("R " + token(challenge++)); controller->poll(); frame(); last = receive();
        check(!last.empty() && YAML::Load(last)["type"].as<std::string>() == "state", "real provider to controller state packet");
        return last;
    }
    void queue(const std::string &action, const std::string &choice = "")
    {
        const YAML::Node state = YAML::Load(last);
        request = "E " + token(command++) + " 1 " + epoch + " " + state["capability"].as<std::string>() + " " +
            state["revision"].as<std::string>() + " " + action + (choice.empty() ? "" : " " + choice);
        send(request); controller->poll(); check(receive().empty(), "queued E waits for native frame");
    }
    std::string events()
    {
        send("V " + token(challenge++) + " " + epoch + " 0 16"); controller->poll();
        const std::string value = receive(); check(!value.empty(), "actual retained events response"); return value;
    }
};
