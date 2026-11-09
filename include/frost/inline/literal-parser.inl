/*
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_LITERAL_PARSER_H_HEADER_GUARD
#	error "Must be included from frost/literal-parser.h!"
#endif // FROST_LITERAL_PARSER_H_HEADER_GUARD

namespace frost
{
	inline bool LiteralParser::isOk() const
	{
		return m_ok;
	}

	inline const StringView& LiteralParser::getLiteral() const
	{
		return m_literal;
	}

	inline LiteralParser::Type LiteralParser::getType() const
	{
		return m_type;
	}

} // namespace frost
