#include <clocale>
#include <deque>
#include <iostream>
#include <utility>

#include "DistrhoPlugin.hpp"
#include "src/DistrhoPluginInternal.hpp"
#include "DistrhoPluginInfo.h"


START_NAMESPACE_DISTRHO
Plugin* createPlugin();
END_NAMESPACE_DISTRHO


#define BOOL(WHOMST) Syntax().keyword((WHOMST) ? "true" : "false")


enum Token {
    Colon,
    Comma,
    PopArray,
    PopObject,
    PushArray,
    PushObject,
    Whatever,
};


class Syntax {
public:
    std::deque<Token> tokens;
    std::deque<std::string> strings;

    Syntax() {}
    Syntax(const Syntax&) = delete;
    virtual ~Syntax() {}

    void output() {
        while (tokens.size() != 0) {
            Token token = tokens.front();
            switch (token) {
                case Colon: std::cout << ":"; break;
                case Comma: std::cout << ","; break;
                case PopArray: std::cout << "]"; break;
                case PopObject: std::cout << "}"; break;
                case PushArray: std::cout << "["; break;
                case PushObject: std::cout << "{"; break;
                case Whatever: std::cout << strings.front(); strings.pop_front(); break;
            }
            tokens.pop_front();
        }
    }

    Syntax& merge(const Syntax& other) {
        tokens.insert(tokens.end(), other.tokens.begin(), other.tokens.end());
        strings.insert(strings.end(), other.strings.begin(), other.strings.end());
        return *this;
    }

    Syntax& object(class Object& object);
    Syntax& array(class Array& array);
    Syntax& string(std::string string);
    Syntax& number(double number);
    Syntax& keyword(std::string keyword);
};


class Array : public Syntax {
public:
    Array() {}
    Array(const Array&) = delete;
    virtual ~Array() {}

    Array& item(Syntax& item) {
        if (tokens.size() != 0) {
            tokens.push_back(Comma);
        }
        merge(item);
        return *this;
    }
};


class Object : public Syntax {
public:
    Object() {}
    Object(const Object&) = delete;
    virtual ~Object() {}

    Object& item(std::string key, Syntax& item) {
        if (tokens.size() != 0) {
            tokens.push_back(Comma);
        }
        merge(Syntax().string(key));
        tokens.push_back(Colon);
        merge(item);
        return *this;
    }
};


Syntax& Syntax::object(Object& object) {
    tokens.push_back(PushObject);
    merge(object);
    tokens.push_back(PopObject);
    return *this;
}


Syntax& Syntax::array(Array& array) {
    tokens.push_back(PushArray);
    merge(array);
    tokens.push_back(PopArray);
    return *this;
}


Syntax& Syntax::string(std::string string) {
    tokens.push_back(Whatever);
    strings.push_back(std::string("\"") + string + std::string("\""));
    return *this;
}


Syntax& Syntax::number(double number) {
    tokens.push_back(Whatever);
    strings.push_back(std::to_string(number));
    return *this;
}


Syntax& Syntax::keyword(std::string keyword) {
    tokens.push_back(Whatever);
    strings.push_back(keyword);
    return *this;
}


int main(int argc, char** argv) {
    std::setlocale(LC_ALL, "C");

    d_lastBufferSize = 256;
    d_lastSampleRate = 44100.0;

    PluginExporter plugin;

    Object obj;
    obj.item("uri", Syntax().string(DISTRHO_PLUGIN_URI));
    obj.item("name", Syntax().string(DISTRHO_PLUGIN_NAME));
    obj.item("dllname", Syntax().string(BITROT_BINARY_NAME));
    obj.item("ttlname", Syntax().string(BITROT_TTL_NAME));
#if defined(DISTRHO_PLUGIN_REPLACED_URI)
    obj.item("replaced_uri", Syntax().string(DISTRHO_PLUGIN_REPLACED_URI));
#else
    obj.item("replaced_uri", Syntax().keyword("null"));
#endif
    obj.item("description", Syntax().string(plugin.getDescription()));
    obj.item("license", Syntax().string(plugin.getLicense()));
    obj.item("maker", Syntax().string(plugin.getMaker()));
    obj.item("is_rt_safe", BOOL(DISTRHO_PLUGIN_IS_RT_SAFE));

    Object version;
    version.item("major", Syntax().number(BITROT_VERSION_MAJOR));
    version.item("minor", Syntax().number(BITROT_VERSION_MINOR));
    version.item("micro", Syntax().number(BITROT_VERSION_MICRO));
    obj.item("version", Syntax().object(version));

    Array params;
    for (uint32_t i = 0; i < plugin.getParameterCount(); ++i) {
        Object param;

        param.item(
            "direction",
            Syntax().string(
                plugin.isParameterOutput(i) ? "output" : "input"
            )
        );

        param.item("symbol", Syntax().string(
            plugin.getParameterSymbol(i).buffer()));
        param.item("name", Syntax().string(
            plugin.getParameterName(i).buffer()));

        const ParameterRanges& ranges(plugin.getParameterRanges(i));
        param.item("minimum", Syntax().number(ranges.min));
        param.item("maximum", Syntax().number(ranges.max));
        param.item("default", Syntax().number(ranges.def));

        const uint32_t hints(plugin.getParameterHints(i));
        param.item("trigger", BOOL(
            (hints & kParameterIsTrigger) == kParameterIsTrigger));
        param.item("toggled", BOOL(hints & kParameterIsBoolean));
        param.item("integer", BOOL(hints & kParameterIsInteger));
        param.item("logarithmic", BOOL(hints & kParameterIsLogarithmic));
        param.item("automatable", BOOL(hints & kParameterIsAutomatable));

        params.item(Syntax().object(param));
    }
    obj.item("params", Syntax().array(params));

    Syntax().object(obj).output();
    std::cout << std::endl;

    return EXIT_SUCCESS;
}
