using ClangSharp;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public enum ETraversalBehaviour
    {
        /**
		*	Recursively traverse the entities contained in the current entity, using
		*	the same visitor and environment.
		*/
        Recurse = 0,

		/**
		*	Continues the entities traversal with the next sibling entity without visiting
		*	nested entities.
		*/
		Continue,

		/**
		*	Cancel the traversal on the siblings of the same entity type and resume
		*	it with the next sibling of a different type.
		*/
		Break,

		/**
		*	Abort the entity traversal but makes the generateCode method return true (success).
		*/
		AbortWithSuccess,

		/**
		*	Abort the entity traversal and make the generateCode method return false (failure).
		*/
		AbortWithFailure
    };

    public abstract class CppReflectGenerator
    {
        protected HeaderFile HeaderFile { get; }
        protected CodeGenCppReflectSettings Settings { get; }

        public CppReflectGenerator(HeaderFile headerFile, CodeGenCppReflectSettings settings)
        {
            HeaderFile = headerFile;
            Settings = settings;
        }

		public ETraversalBehaviour ForeachEntityPair()
		{
			ETraversalBehaviour result = ETraversalBehaviour.Break;
			var parsingResult = HeaderFile.FileParsingResult;
			if (parsingResult == null || parsingResult.Errors.Count > 0)
				return ETraversalBehaviour.Break;

            foreach (var classInfo in parsingResult.Classes)
            {
                result = EntityForeachVisitInStruct(classInfo);
                if (result == ETraversalBehaviour.Break)
                    break;
                else if (result == ETraversalBehaviour.AbortWithSuccess || result == ETraversalBehaviour.AbortWithFailure)
                    return result;
            }

            foreach (var structInfo in parsingResult.Structs)
            {
                result = EntityForeachVisitInStruct(structInfo);
                if (result == ETraversalBehaviour.Break)
                    break;
                else if (result == ETraversalBehaviour.AbortWithSuccess || result == ETraversalBehaviour.AbortWithFailure)
                    return result;
            }

            foreach (var enumInfo in parsingResult.Enums)
			{
				result = EntityForeachVisitInEnum(enumInfo);
                if (result == ETraversalBehaviour.Break)
					break;
				else if (result == ETraversalBehaviour.AbortWithSuccess || result == ETraversalBehaviour.AbortWithFailure)
					return result;
			}
			return result;
		}

		protected ETraversalBehaviour EntityForeachVisitInEnum(EnumInfo enumInfo)
		{
            var ret = GenerateCodeForEntity(enumInfo);
			if (ret != ETraversalBehaviour.Recurse)
				return ret;

            foreach (var enumValueInfo in enumInfo.EnumValues)
            {
                ret = GenerateCodeForEntity(enumValueInfo);

				if (ret == ETraversalBehaviour.Break)
					break;
				else if (ret == ETraversalBehaviour.AbortWithSuccess || ret == ETraversalBehaviour.AbortWithFailure)
					return ret;
            }

			return ETraversalBehaviour.Recurse;
        }

        protected ETraversalBehaviour EntityForeachVisitInStruct(StructClassInfo structClassInfo)
        {
            var ret = GenerateCodeForEntity(structClassInfo);
            if (ret != ETraversalBehaviour.Recurse)
                return ret;

            return ETraversalBehaviour.Recurse;
        }

        protected abstract ETraversalBehaviour GenerateCodeForEntity(EntityInfo entityInfo);

        public abstract void Generate(CodeGenFactory CodeGenFactory);
    }
}
