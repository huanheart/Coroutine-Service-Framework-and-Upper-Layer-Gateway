#include "jwt_utils.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cstring>
#include <vector>
#include <chrono>

namespace {
std::string b64url_to_b64(const std::string& in) {
    std::string out = in;
    for (auto& c : out) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (out.size() % 4 != 0) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in_b64) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* bio = BIO_new_mem_buf(in_b64.data(), in_b64.size());
    bio = BIO_push(b64, bio);
    std::vector<unsigned char> buf(in_b64.size());
    int len = BIO_read(bio, buf.data(), buf.size());
    BIO_free_all(bio);
    if (len <= 0) return std::string();
    return std::string(reinterpret_cast<char*>(buf.data()), len);
}

std::string base64_encode(const std::string& data) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bio);
    BIO_write(b64, data.data(), data.size());
    BIO_flush(b64);
    BUF_MEM* mem;
    BIO_get_mem_ptr(b64, &mem);
    std::string out(mem->data, mem->length);
    BIO_free_all(b64);
    return out;
}

std::string b64_to_b64url(const std::string& in) {
    std::string out = in;
    for (auto& c : out) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    while (!out.empty() && out.back() == '=') out.pop_back();
    return out;
}

bool extract_string(const std::string& json, const std::string& key, std::string& out) {
    std::string k = "\"" + key + "\"";
    size_t p = json.find(k);
    if (p == std::string::npos) return false;
    size_t c = json.find(':', p + k.size());
    if (c == std::string::npos) return false;
    size_t s = json.find('"', c + 1);
    if (s == std::string::npos) return false;
    size_t e = json.find('"', s + 1);
    if (e == std::string::npos) return false;
    out = json.substr(s + 1, e - s - 1);
    return true;
}

bool extract_number(const std::string& json, const std::string& key, int64_t& out) {
    std::string k = "\"" + key + "\"";
    size_t p = json.find(k);
    if (p == std::string::npos) return false;
    size_t c = json.find(':', p + k.size());
    if (c == std::string::npos) return false;
    size_t s = c + 1;
    while (s < json.size() && (json[s] == ' ' || json[s] == '\t')) ++s;
    size_t e = s;
    while (e < json.size() && (json[e] >= '0' && json[e] <= '9')) ++e;
    if (e == s) return false;
    out = std::strtoll(json.substr(s, e - s).c_str(), nullptr, 10);
    return true;
}
}

namespace jwt {
bool verify_hs256(const std::string& token,
                  const std::string& secret,
                  const std::string& issuer,
                  const std::string& audience,
                  std::string& err) {
    size_t p1 = token.find('.');
    if (p1 == std::string::npos) { err = "format"; return false; }
    size_t p2 = token.find('.', p1 + 1);
    if (p2 == std::string::npos) { err = "format"; return false; }
    std::string h = token.substr(0, p1);
    std::string p = token.substr(p1 + 1, p2 - p1 - 1);
    std::string s = token.substr(p2 + 1);
    std::string header = base64_decode(b64url_to_b64(h));
    std::string payload = base64_decode(b64url_to_b64(p));
    std::string sig = base64_decode(b64url_to_b64(s));
    if (header.empty() || payload.empty() || sig.empty()) { err = "b64"; return false; }
    if (header.find("\"alg\":\"HS256\"") == std::string::npos) { err = "alg"; return false; }
    std::string signing_input = h + "." + p;
    unsigned int len = 0;
    unsigned char mac[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(),
         secret.data(),
         secret.size(),
         reinterpret_cast<const unsigned char*>(signing_input.data()),
         signing_input.size(),
         mac, &len);
    if (sig.size() != len || std::memcmp(sig.data(), mac, len) != 0) { err = "sig"; return false; }
    int64_t exp = 0;
    if (extract_number(payload, "exp", exp)) {
        int64_t now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (exp < now) { err = "exp"; return false; }
    }
    if (!issuer.empty()) {
        std::string iss;
        if (!extract_string(payload, "iss", iss) || iss != issuer) { err = "iss"; return false; }
    }
    if (!audience.empty()) {
        std::string aud;
        if (!extract_string(payload, "aud", aud) || aud != audience) { err = "aud"; return false; }
    }
    return true;
}

bool get_exp(const std::string& token, int64_t& exp_out) {
    size_t p1 = token.find('.');
    if (p1 == std::string::npos) return false;
    size_t p2 = token.find('.', p1 + 1);
    if (p2 == std::string::npos) return false;
    std::string p = token.substr(p1 + 1, p2 - p1 - 1);
    std::string payload = base64_decode(b64url_to_b64(p));
    if (payload.empty()) return false;
    int64_t exp = 0;
    if (!extract_number(payload, "exp", exp)) return false;
    exp_out = exp;
    return true;
}

std::string sign_hs256(const std::string& secret,
                       const std::string& issuer,
                       const std::string& audience,
                       int64_t exp) {
    std::string header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    std::string payload = "{\"iss\":\"" + issuer + "\",\"aud\":\"" + audience + "\",\"exp\":" + std::to_string(exp) + "}";
    std::string h_b64 = b64_to_b64url(base64_encode(header));
    std::string p_b64 = b64_to_b64url(base64_encode(payload));
    std::string signing_input = h_b64 + "." + p_b64;
    unsigned int len = 0;
    unsigned char mac[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(),
         secret.data(),
         secret.size(),
         reinterpret_cast<const unsigned char*>(signing_input.data()),
         signing_input.size(),
         mac, &len);
    std::string sig(reinterpret_cast<char*>(mac), len);
    std::string s_b64 = b64_to_b64url(base64_encode(sig));
    return signing_input + "." + s_b64;
}
}
