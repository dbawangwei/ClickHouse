#include "NoEscapingValueStateHandler.h"
#include <Functions/keyvaluepair/src/impl/state/strategies/util/CharacterFinder.h>
#include <Functions/keyvaluepair/src/impl/state/strategies/util/NeedleFactory.h>

namespace DB
{

NoEscapingValueStateHandler::NoEscapingValueStateHandler(Configuration extractor_configuration_)
    : extractor_configuration(std::move(extractor_configuration_))
{
    read_needles = NeedleFactory::getReadNeedles(extractor_configuration);
    read_quoted_needles = NeedleFactory::getReadQuotedNeedles(extractor_configuration);
}

NextState NoEscapingValueStateHandler::wait(std::string_view file) const
{
    const auto & [key_value_delimiter, quoting_character, pair_delimiters]
        = extractor_configuration;

    size_t pos = 0;
    if (pos < file.size())
    {
        const auto current_character = file[pos];

        if (quoting_character == current_character)
        {
            return {pos + 1u, State::READING_QUOTED_VALUE};
        }
        else if (key_value_delimiter == current_character)
        {
            return {pos, State::WAITING_KEY};
        }
        else
        {
            return {pos, State::READING_VALUE};
        }
    }

    return {file.size(), State::READING_VALUE};
}

NextState NoEscapingValueStateHandler::read(std::string_view file, ElementType & value) const
{
    size_t pos = 0;
    auto start_index = pos;

    value = {};

    BoundsSafeCharacterFinder finder;

    const auto & [key_value_delimiter, quoting_character, pair_delimiters]
        = extractor_configuration;

    while (auto character_position_opt = finder.findFirst(file, pos, read_needles))
    {
        auto character_position = *character_position_opt;
        auto character = file[character_position];
        auto next_pos = character_position + 1u;

        if (key_value_delimiter == character)
        {
            return {next_pos, State::WAITING_KEY};
        }
        else if (std::find(pair_delimiters.begin(), pair_delimiters.end(), character) != pair_delimiters.end())
        {
            value = createElement(file, start_index, character_position);
            return {next_pos, State::FLUSH_PAIR};
        }

        pos = next_pos;
    }

    // TODO: do I really need the below logic?
    // this allows empty values at the end
    value = createElement(file, start_index, file.size());
    return {file.size(), State::FLUSH_PAIR};
}

NextState NoEscapingValueStateHandler::readQuoted(std::string_view file, ElementType & value) const
{
    size_t pos = 0;
    auto start_index = pos;

    value = {};

    BoundsSafeCharacterFinder finder;

    const auto quoting_character = extractor_configuration.quoting_character;

    while (auto character_position_opt = finder.findFirst(file, pos, read_quoted_needles))
    {
        auto character_position = *character_position_opt;
        auto character = file[character_position];
        auto next_pos = character_position + 1u;

        if (quoting_character == character)
        {
            value = createElement(file, start_index, character_position);

            std::cerr << "NoEscapingValueStateHandler::readQuoted Going to consume up to: «" << fancyQuote(file.substr(0, next_pos)) << " to " << fancyQuote(file.substr(next_pos)) << std::endl;
            return {next_pos, State::FLUSH_PAIR};
        }

        pos = next_pos;
    }

    return {file.size(), State::END};
}

}
