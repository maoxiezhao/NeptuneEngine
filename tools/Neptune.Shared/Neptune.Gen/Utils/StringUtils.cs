using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Neptune.Gen
{
    /// <summary>
    /// Provides a cache of StringBuilders
    /// </summary>
    public class StringBuilderCache
    {

        /// <summary>
        /// Cache of StringBuilders with large initial buffer sizes
        /// </summary>
        public static readonly StringBuilderCache Big = new(256, 256 * 1024);

        /// <summary>
        /// Cache of StringBuilders with small initial buffer sizes
        /// </summary>
        public static readonly StringBuilderCache Small = new(256, 1 * 1024);

        /// <summary>
        /// Capacity of the cache
        /// </summary>
        private readonly int _capacity;

        /// <summary>
        /// Initial buffer size for new StringBuilders.  Resetting StringBuilders might result
        /// in the initial chunk size being smaller.
        /// </summary>
        private readonly int _initialBufferSize;

        /// <summary>
        /// Stack of cached StringBuilders
        /// </summary>
        private readonly Stack<StringBuilder> _stack;

        /// <summary>
        /// Create a new StringBuilder cache
        /// </summary>
        /// <param name="capacity">Maximum number of StringBuilders to cache</param>
        /// <param name="initialBufferSize">Initial buffer size for newly created StringBuilders</param>
        public StringBuilderCache(int capacity, int initialBufferSize)
        {
            _capacity = capacity;
            _initialBufferSize = initialBufferSize;
            _stack = new Stack<StringBuilder>(_capacity);
        }

        /// <summary>
        /// Borrow a StringBuilder from the cache.
        /// </summary>
        /// <returns></returns>
        public StringBuilder Borrow()
        {
            lock (_stack)
            {
                if (_stack.Count > 0)
                {
                    return _stack.Pop();
                }
            }

            return new StringBuilder(_initialBufferSize);
        }

        /// <summary>
        /// Return a StringBuilder to the cache
        /// </summary>
        /// <param name="builder">The builder being returned</param>
        public void Return(StringBuilder builder)
        {
            // Sadly, clearing the builder (sets length to 0) will reallocate chunks.
            builder.Clear();
            lock (_stack)
            {
                if (_stack.Count < _capacity)
                {
                    _stack.Push(builder);
                }
            }
        }
    }

    /// <summary>
    /// Structure to automate the borrowing and returning of a StringBuilder.
    /// Use some form of a "using" pattern.
    /// </summary>
    public struct BorrowStringBuilder : IDisposable
    {

        /// <summary>
        /// Owning cache
        /// </summary>
        private StringBuilderCache Cache { get; }

        /// <summary>
        /// Borrowed string builder
        /// </summary>
        public StringBuilder StringBuilder { get; }

        /// <summary>
        /// Borrow a string builder from the given cache
        /// </summary>
        /// <param name="cache">String builder cache</param>
        public BorrowStringBuilder(StringBuilderCache cache)
        {
            Cache = cache;
            StringBuilder = Cache.Borrow();
        }

        /// <summary>
        /// Return the string builder to the cache
        /// </summary>
        public void Dispose()
        {
            Cache.Return(StringBuilder);
        }
    }
}
