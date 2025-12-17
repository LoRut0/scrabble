#include "Cors.hpp"
#include <userver/server/http/http_response.hpp>

using namespace userver;

namespace services::cors {

CorsHandler::CorsHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
    : HttpHandlerBase(config, context) { };

std::string CorsHandler::HandleRequest(server::http::HttpRequest& request, server::request::RequestContext&) const
{
    server::http::HttpResponse& resp = request.GetHttpResponse();
    resp.SetHeader(std::string { "Access-Control-Allow-Origin" }, "*");
    resp.SetHeader(std::string { "Access-Control-Allow-Headers" }, "Content-Type");
    resp.SetHeader(std::string { "Access-Control-Allow-Methods" }, "GET, POST, OPTIONS");
    return {};
}

}
