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
		swarm::url url("http://xn--d1abbgf6aiiy.xn--p1ai/%D0%BD%D0%BE%D0%B2%D0%BE%D1%81%D1%82%D0%B8");
		std::cout << (url.is_valid() ? "valid" : "not valid") << " " << url.to_string() << std::endl;
		std::cout << url.host() << " " << url.path() << std::endl;
	}
	{
		swarm::url url = swarm::url::from_user_input("http://президент.рф/новости");
		std::cout << (url.is_valid() ? "valid" : "not valid") << " " << url.to_string() << std::endl;
		std::cout << ((url.to_string() == "http://xn--d1abbgf6aiiy.xn--p1ai/%D0%BD%D0%BE%D0%B2%D0%BE%D1%81%D1%82%D0%B8") ? "encoded correctly" : "FAIL") << std::endl;
		std::cout << "host: " << url.host() << std::endl;
		std::cout << "path: " << url.path() << std::endl;
	}
	{
		swarm::url url = swarm::url::from_user_input("http://xn--d1abbgf6aiiy.xn--p1ai/%D0%BD%D0%BE%D0%B2%D0%BE%D1%81%D1%82%D0%B8?%D0%BF%D1%80%D0%B8%D0%B2%D0%B5%D1%82=%D0%BF%D0%BE%D0%BA%D0%B0#%D1%82%D0%B5%D0%BB%D0%B5%D0%B3%D1%80%D0%B0%D0%BC%D0%BC%D1%8B");
		std::cout << url.is_valid() << " " << url.to_string() << std::endl;
		std::cout << (url.query().item_value("привет") ? *url.query().item_value("привет") : "MISSED") << std::endl;
		std::cout << url.to_human_readable() << std::endl;
	}
	{
		swarm::url base_url("http://doc.ReveRBrain.cOm/elliptics:what/smth/");
		swarm::url relative_url("../thevoid:thevoid");
		std::cout << "domain: " << relative_url.host() << std::endl;
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
	{
		swarm::url url;
		url.set_host("example.org");
		url.set_path("/hello");
		std::cout << url.to_string() << std::endl;
	}
	{
		swarm::url url;
		url.set_scheme("http");
		url.set_host("example.org");
		url.set_path("hello");
		std::cout << url.to_string() << std::endl;
	}
}
