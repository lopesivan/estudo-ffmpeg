// unix_command.hpp
#pragma once

#include <cstdlib>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

// --------------------------------------------------
// 1) Conceito: OutputIterator que aceita std::string
// --------------------------------------------------
template <class It>
concept StringOutputIterator =
    std::output_iterator<It, std::string>;

// --------------------------------------------------
// 2) Núcleo genérico: comando Unix
// --------------------------------------------------
class UnixCommand
{
public:
    UnixCommand (std::string              program,
                 std::vector<std::string> args = {})
        : program_ (std::move (program)), args_ (std::move (args))
    {
    }

    const std::string& program() const noexcept
    {
        return program_;
    }
    const std::vector<std::string>& args() const noexcept
    {
        return args_;
    }

    template <StringOutputIterator Out> void to_args (Out out) const
    {
        *out++ = program_;
        for (const auto& arg : args_)
        {
            *out++ = arg;
        }
    }

    std::string to_command_line() const
    {
        std::vector<std::string> all;
        to_args (std::back_inserter (all));

        std::ostringstream oss;
        bool               first = true;

        for (const auto& arg : all)
        {
            if (!first)
            {
                oss << ' ';
                // oss << '\n'; // pula linha entre os argumentos
            }

            first = false;
            oss << quote_arg (arg);
        }

        return oss.str();
    }

    int run() const
    {
        std::string cmd = to_command_line();
        std::cout << "[UnixCommand] " << cmd << '\n';

        return std::system (to_command_line().c_str());
    }

private:
    std::string              program_;
    std::vector<std::string> args_;

    static std::string quote_arg (const std::string& arg)
    {
        std::string result;
        result.reserve (arg.size() + 2);

        result.push_back ('\'');

        for (char c : arg)
        {
            if (c == '\'')
            {
                result += "'\\''";
            }
            else
            {
                result.push_back (c);
            }
        }

        result.push_back ('\'');

        return result;
    }
};

// --------------------------------------------------
// 3) Algoritmo genérico auxiliar
// --------------------------------------------------
template <StringOutputIterator Out>
void append_args (Out out, const std::vector<std::string>& args)
{
    for (const auto& arg : args)
    {
        *out++ = arg;
    }
}

// --------------------------------------------------
// 4) Concept para qualquer descrição de tarefa
// --------------------------------------------------
template <class Job, class Out>
concept CommandJob =
    StringOutputIterator<Out>&&
    requires (const Job& job, Out out)
{
    make_args (job, out);
};

// --------------------------------------------------
// 5) Adaptador genérico: Job -> UnixCommand
// --------------------------------------------------
template <class Job>
UnixCommand make_unix_command (const std::string& program,
                               const Job&         job)
{
    std::vector<std::string> args;
    make_args (job, std::back_inserter (args));
    return UnixCommand (program, std::move (args));
}
