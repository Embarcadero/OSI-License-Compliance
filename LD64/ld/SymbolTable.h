/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-*
 *
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __SYMBOL_TABLE_H__
#define __SYMBOL_TABLE_H__

#ifdef BORLAND_LD
#include "stubs.h"
#include "win_misc.h"
#include "win_fcntl.h"
#include "win_stat.h"
#include "win_stdio.h"
#include "win_unistd.h"
#include "win_stdlib.h"
#include "win_string.h"
#include "win_mman.h"
#else
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#endif
#include <mach/mach_time.h>
#include <mach/vm_statistics.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#ifndef BORLAND_LD
#include <dlfcn.h>
#else
#include <win_dlfcn.h>
#endif
#include <mach-o/dyld.h>

#include <vector>
#include <unordered_map>
#include <string_view>

#include "Options.h"
#include "ld.hpp"
#include "Containers.h"

namespace ld {
namespace tool {


class SymbolTable : public ld::IndirectBindingTable
{
public:
	typedef uint32_t IndirectBindingSlot;

private:
	using NameToSlot = StringViewMap<IndirectBindingSlot>;

	class ContentFuncs {
	public:
		size_t	operator()(const ld::Atom*) const;
		bool	operator()(const ld::Atom* left, const ld::Atom* right) const;
	};
	typedef std::unordered_map<const ld::Atom*, IndirectBindingSlot, ContentFuncs, ContentFuncs> ContentToSlot;

	class ReferencesHashFuncs {
	public:
		size_t	operator()(const ld::Atom*) const;
		bool	operator()(const ld::Atom* left, const ld::Atom* right) const;
	};
	typedef std::unordered_map<const ld::Atom*, IndirectBindingSlot, ReferencesHashFuncs, ReferencesHashFuncs> ReferencesToSlot;

	class CStringHashFuncs {
	public:
		size_t	operator()(const ld::Atom*) const;
		bool	operator()(const ld::Atom* left, const ld::Atom* right) const;
	};
	typedef std::unordered_map<const ld::Atom*, IndirectBindingSlot, CStringHashFuncs, CStringHashFuncs> CStringToSlot;

	class UTF16StringHashFuncs {
	public:
		size_t	operator()(const ld::Atom*) const;
		bool	operator()(const ld::Atom* left, const ld::Atom* right) const;
	};
	typedef std::unordered_map<const ld::Atom*, IndirectBindingSlot, UTF16StringHashFuncs, UTF16StringHashFuncs> UTF16StringToSlot;

	using SlotToName = Map<IndirectBindingSlot, std::string_view>;
	using NameToMap = CStringMap<CStringToSlot*>;
    
    typedef std::vector<const ld::Atom *> DuplicatedSymbolAtomList;
    typedef std::map<const char *, DuplicatedSymbolAtomList * > DuplicateSymbols;
	
public:

	class byNameIterator {
	public:
		byNameIterator&			operator++() { ++_nameTableIterator; return *this; }
		byNameIterator			operator++(int) { auto cpy = *this; ++_nameTableIterator; return cpy; }

		const ld::Atom*			operator*() { return _slotTable[_nameTableIterator->second]; }
		bool					operator!=(const byNameIterator& lhs) { return _nameTableIterator != lhs._nameTableIterator; }

	private:
		friend class SymbolTable;
								byNameIterator(NameToSlot::iterator it, std::vector<const ld::Atom*>& indirectTable)
									: _nameTableIterator(it), _slotTable(indirectTable) {} 
		
		NameToSlot::iterator			_nameTableIterator;
		std::vector<const ld::Atom*>&	_slotTable;
	};
	
						SymbolTable(const Options& opts, std::vector<const ld::Atom*>& ibt, size_t inputFileCount);

	bool				add(const ld::Atom& atom, Options::Treatment duplicates);
	IndirectBindingSlot	findSlotForName(const std::string_view& name);
	IndirectBindingSlot	findSlotForContent(const ld::Atom* atom, const ld::Atom** existingAtom);
	IndirectBindingSlot	findSlotForReferences(const ld::Atom* atom, const ld::Atom** existingAtom);
	const ld::Atom*		atomForSlot(IndirectBindingSlot s)	{ return _indirectBindingTable[s]; }
	const ld::Atom*		atomForName(const std::string_view& name) const;
	unsigned int		updateCount()						{ return _indirectBindingTable.size(); }
	void				undefines(std::vector<std::string_view>& undefines);
	void				tentativeDefs(std::vector<std::string_view>& undefines);
	void				mustPreserveForBitcode(std::unordered_set<const char*>& syms);
	void				removeDeadAtoms();
	bool				hasName(const std::string_view& name);
	bool				hasTentativeDefinitions()	{ return _hasTentativeDefinitions; }
	byNameIterator		begin()								{ return byNameIterator(_byNameTable.begin(),_indirectBindingTable); }
	byNameIterator		end()								{ return byNameIterator(_byNameTable.end(),_indirectBindingTable); }
	const std::vector<const ld::Atom*>& atoms() const { return _indirectBindingTable; }

	void				printStatistics();
	void				removeDeadUndefs(std::vector<const ld::Atom *>& allAtoms, const std::unordered_set<const ld::Atom*>& keep);

	// from ld::IndirectBindingTable
	virtual const char*			indirectName(IndirectBindingSlot slot) const;
	virtual const ld::Atom*		indirectAtom(IndirectBindingSlot slot) const;
    
    // Prints the duplicated symbols to stderr and throws. Only valid to call if hasDuplicateSymbols() returns true.
    void                checkDuplicateSymbols() const;

	static void			markCoalescedAway(const ld::Atom* atom);

private:
	bool					addByName(const ld::Atom& atom, Options::Treatment duplicates);
	bool					addByContent(const ld::Atom& atom);
	bool					addByReferences(const ld::Atom& atom);

    // Tracks duplicated symbols. Each call adds file to the list of files defining symbol.
    // The file list is uniqued per symbol, so calling multiple times for the same symbol/file pair is permitted.
    void                    addDuplicateSymbol(DuplicateSymbols& dups, const char* symbol, const ld::Atom* atom);
	void 					addDuplicateSymbolError(const char* name, const ld::Atom* atom);
	void 					addDuplicateSymbolWarning(const char* name, const ld::Atom* atom);

	const Options&					_options;
	NameToSlot						_byNameTable;
	SlotToName						_byNameReverseTable;
	ContentToSlot					_literal4Table;
	ContentToSlot					_literal8Table;
	ContentToSlot					_literal16Table;
	UTF16StringToSlot				_utf16Table;
	CStringToSlot					_cstringTable;
	NameToMap						_nonStdCStringSectionToMap;
	ReferencesToSlot				_nonLazyPointerTable;
	ReferencesToSlot				_threadPointerTable;
	ReferencesToSlot				_cfStringTable;
	ReferencesToSlot				_objc2ClassRefTable;
	ReferencesToSlot				_pointerToCStringTable;
	std::vector<const ld::Atom*>&	_indirectBindingTable;
	bool							_hasTentativeDefinitions;
	
    DuplicateSymbols                _duplicateSymbolErrors;
    DuplicateSymbols                _duplicateSymbolWarnings;
};

} // namespace tool 
} // namespace ld 


#endif // __SYMBOL_TABLE_H__
