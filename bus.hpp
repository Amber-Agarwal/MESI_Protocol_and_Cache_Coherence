enum class request_type
{
    BusRd,
    BusRdX
};

struct bus_request
{   int core_id;
    unsigned int address;
    request_type type;
};

class Bus {

};