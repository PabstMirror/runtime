#pragma once
#include "../../runtime/parser/config.h"
#include "../../runtime/logging.h"
#include "../../runtime/diagnostics/diag_info.h"
#include "../../runtime/confighost.h"
#include "../../runtime/fileio.h"

#include <string>
#include <string_view>
#include <vector>

namespace sqf::parser::config
{
	class default: public ::sqf::runtime::parser::config, public CanLog
	{
	public:
		enum class nodetype
		{
			NA = 0,
			NODELIST,
			NODE,
			CONFIGNODE,
			CONFIGNODE_PARENTIDENT,
			VALUENODE,
			VALUENODE_ADDARRAY,
			DELETENODE,
			STRING,
			NUMBER,
			HEXNUMBER,
			LOCALIZATION,
			ARRAY,
			VALUE
		};
		struct astnode
		{
			size_t offset;
			size_t length;
			size_t line;
			size_t col;
			std::string content;
			::sqf::runtime::fileio::pathinfo path;
			nodetype kind;
			std::vector<astnode> children;

			astnode() : offset(0), length(0), line(0), col(0), kind(nodetype::NA) {}
		};
	private:
		class instance
		{
		private:
			default& owner;
		public:
			instance(
				default& owner,
				std::string_view contents,
				::sqf::runtime::fileio::pathinfo file
				) : owner(owner),
				m_contents(contents),
				m_file(file)
			{
				::sqf::runtime::diagnostics::diag_info dinf(1, 0, 0, m_file, {});
				m_info = dinf;
			}
			::sqf::runtime::diagnostics::diag_info m_info;
			std::string_view m_contents;
			::sqf::runtime::fileio::pathinfo m_file;

			void skip();

			size_t endchr(size_t off);
			size_t identifier(size_t off);
			size_t operator_(size_t off);
			size_t hexadecimal(size_t off);
			size_t numsub(size_t off);
			size_t num(size_t off);
			size_t anytext(size_t off);
			bool NODELIST_start(size_t curoff);
			void NODELIST(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool NODE_start(size_t curoff);
			void NODE(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool CONFIGNODE_start(size_t curoff);
			void CONFIGNODE(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool DELETENODE_start(size_t curoff);
			void DELETENODE(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool VALUENODE_start(size_t curoff);
			void VALUENODE(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool STRING_start(size_t curoff);
			void STRING(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool NUMBER_start(size_t curoff);
			void NUMBER(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool LOCALIZATION_start(size_t curoff);
			void LOCALIZATION(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool ARRAY_start(size_t curoff);
			void ARRAY(::sqf::parser::config::default::astnode& root, bool& errflag);
			bool VALUE_start(size_t curoff);
			void VALUE(::sqf::parser::config::default::astnode& root, bool& errflag);

			::sqf::parser::config::default::astnode parse(bool& errflag);
		};
		bool apply_to_confighost(::sqf::parser::config::default::astnode& node, ::sqf::runtime::confighost& confighost, ::sqf::runtime::confighost::config& parent);
	public:
		default(Logger& logger) : CanLog(logger) { }
		virtual ~default() override { };

		virtual bool check_syntax(std::string contents, ::sqf::runtime::fileio::pathinfo pathinfo) override
		{
			instance i(*this, contents, pathinfo);
			bool success = false;
			auto root = i.parse(success);
			return success;
		}
		virtual bool parse(::sqf::runtime::confighost& target, std::string contents, ::sqf::runtime::fileio::pathinfo pathinfo) override
		{
			instance i(*this, contents, pathinfo);
			bool success = false;
			auto root = i.parse(success);
			if (!success)
			{
				return {};
			}
			return apply_to_confighost(root, target, target.root());
		}
	};
}