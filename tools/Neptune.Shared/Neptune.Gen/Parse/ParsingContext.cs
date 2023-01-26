using ClangSharp.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public enum EAccessSpecifier
    {
        Invalid,
		Public,
		Protected,
		Private
	};

    public class ParsingContext
    {
        public ParsingContext? Parent = null;
        public CXCursor RootCursor = CXCursor.Null;
        public ParsingResultBase? ParsingResult;
        public ParsingSettings? ParsingSettings;
        public AttributeParser? AttributeParser;
        public StructClassTree? StructClassTree;
        public EAccessSpecifier AccessSpecifier = EAccessSpecifier.Invalid;
    }
}
