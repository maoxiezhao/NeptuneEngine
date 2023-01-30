using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    public readonly struct StringView : IEquatable<StringView>
    {
        public ReadOnlyMemory<char> Memory { get; }
        public ReadOnlySpan<char> Span => Memory.Span;

        public StringView(ReadOnlyMemory<char> memory)
        {
            Memory = memory;
        }
        public static bool operator ==(StringView x, StringView y)
        {
            return x.Equals(y);
        }

        public static bool operator !=(StringView x, StringView y)
        {
            return !x.Equals(y);
        }

        public override bool Equals(object? obj)
        {
            return obj is StringView view && Equals(view);
        }

        public bool Equals(StringView other)
        {
            return Equals(other, StringComparison.CurrentCulture);
        }

        public bool Equals(StringView? other, StringComparison comparisonType)
        {
            return other.HasValue && Memory.Span.Equals(other.Value.Memory.Span, comparisonType);
        }

        public override int GetHashCode()
        {
            return string.GetHashCode(Memory.Span);
        }

        public override string ToString()
        {
            return new string(Memory.Span);
        }
    }
}
