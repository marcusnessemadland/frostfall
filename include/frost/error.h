/*
 * Copyright 2010-2026 Branimir Karadzic. All rights reserved.
 * License: See LICENSE in the project root.
 */

#ifndef FROST_ERROR_H_HEADER_GUARD
#define FROST_ERROR_H_HEADER_GUARD

#include "string.h"

///
#define FROST_ERROR_SET(_err, _result, _msg)    \
	FROST_MACRO_BLOCK_BEGIN                     \
		(_err)->setError(_result,  "" _msg); \
	FROST_MACRO_BLOCK_END

///
#define FROST_ERROR_USE_TEMP_WHEN_NULL(_err)                           \
	const frost::Error tmpError; /* It should not be used directly! */ \
	_err = NULL == _err ? const_cast<frost::Error*>(&tmpError) : _err

///
#define FROST_ERROR_SCOPE(_err, ...)                                  \
	FROST_ERROR_USE_TEMP_WHEN_NULL(_err);                             \
	frost::ErrorScope frostErrorScope(const_cast<frost::Error*>(&tmpError), "" __VA_ARGS__)

///
#define FROST_ERROR_RESULT(_err, _code)                       \
	static_assert(_code != 0, "ErrorCode 0 is reserved!"); \
	static constexpr frost::ErrorResult _err = { _code }

/// Set error and begin its context: FROST_ERROR_CONTEXT_BEGIN(_err, _result, _msg, _size[, _skip]).
#define FROST_ERROR_CONTEXT_BEGIN(_err, _result, _msg, ...) \
	FROST_MACRO_BLOCK_BEGIN                                 \
		frost::ErrorContext frostErrorContext(_err, _result, _msg, __VA_ARGS__)

/// Error context writer.
#define FROST_ERROR_CONTEXT_WRITER() \
		(&frostErrorContext)

/// Write to error context.
#define FROST_ERROR_CONTEXT_WRITE(...) \
		frost::write(FROST_ERROR_CONTEXT_WRITER(), frost::ErrorIgnore(), __VA_ARGS__)

/// End error context.
#define FROST_ERROR_CONTEXT_END() \
	FROST_MACRO_BLOCK_END

namespace frost
{
	///
	struct ErrorResult
	{
		uint32_t code;
	};

	///
	class Error
	{
		FROST_CLASS(Error
			, NO_COPY
			);

	public:
		///
		Error();

		///
		void reset();

		///
		void setError(ErrorResult _errorResult, const StringLiteral& _msg, const Location& _location = Location::current() );

		///
		bool isOk() const;

		///
		ErrorResult get() const;

		///
		StringLiteral getMessage() const;

		///
		Location getLocation() const;

		/// The error context attached to this error, or 0 when there is none.
		uint32_t getContext() const;

		///
		bool operator==(const ErrorResult& _rhs) const;

		///
		bool operator!=(const ErrorResult& _rhs) const;

	private:
		friend class ErrorContext;

		const char* m_filePath;
		const char* m_msg;
		uint32_t    m_line;
		int32_t     m_msgLen;
		uint32_t    m_code;
		uint32_t    m_context;
	};

	/// Do nothing even if error is set.
	class ErrorIgnore final : public Error
	{
	public:
		///
		operator Error*();
	};

	/// In debug build assert if error is set.
	class ErrorAssert final : public Error
	{
	public:
		///
		~ErrorAssert();

		///
		operator Error*();
	};

	/// Exit application if error is set.
	class ErrorFatal final : public Error
	{
	public:
		///
		~ErrorFatal();

		///
		operator Error*();
	};

	///
	class ErrorScope
	{
		FROST_CLASS(ErrorScope
			, NO_DEFAULT_CTOR
			, NO_COPY
			);

	public:
		///
		ErrorScope(Error* _err, const StringLiteral& _name);

		///
		~ErrorScope();

		///
		const StringLiteral& getName() const;

	private:
		Error* m_err;
		const StringLiteral m_name;
	};

} // namespace frost

#include "inline/error.inl"

#endif // FROST_ERROR_H_HEADER_GUARD
