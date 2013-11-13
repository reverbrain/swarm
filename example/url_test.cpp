#include <swarm/url.hpp>
#include <iostream>

int main()
{
    using namespace ioremap;
    
    {
        swarm::url url("http://localhost:8080/some/path?a=b&c=d#123");
        std::cout << (url.is_valid() ? "valid" : "not valid") << " " << url.to_string() << std::endl;
    }
    {
        swarm::url url("http://localhost/some/path?a=b&c=d#123");
        std::cout << (url.is_valid() ? "valid" : "not valid") << " " << url.to_string() << std::endl;
    }
    {
        swarm::url base_url("http://doc.ReveRBrain.cOm/elliptics:what/smth/"); 
        swarm::url relative_url("../thevoid:thevoid");
        std::cout << "base: " << base_url.to_string() << std::endl;
        std::cout << "relative: " << relative_url.to_string() << std::endl;
        std::cout << "absolute: " << base_url.resolved(relative_url).to_string() << std::endl;
    }
    {
        swarm::url base_url("http://doc.ReveRBrain.cOm/elliptics:what/smth/"); 
        swarm::url relative_url("/thevoid:thevoid");
        std::cout << "base: " << base_url.to_string() << std::endl;
        std::cout << "relative: " << relative_url.to_string() << std::endl;
        std::cout << "absolute: " << base_url.resolved(relative_url).to_string() << std::endl;
    }
}
