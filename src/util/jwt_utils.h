#ifndef JWT_UTILS_H
#define JWT_UTILS_H
#include <string>
namespace jwt {
bool verify_hs256(const std::string& token,
                  const std::string& secret,
                  const std::string& issuer,
                  const std::string& audience,
                  std::string& err);
bool get_exp(const std::string& token, int64_t& exp);
std::string sign_hs256(const std::string& secret,
                       const std::string& issuer,
                       const std::string& audience,
                       int64_t exp);
}
#endif
