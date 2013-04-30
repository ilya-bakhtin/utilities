using System;
using System.Collections.Generic;
using System.Text;

namespace FindDuplicates
{
    class MultiMap<TKey, TValue> : Dictionary<TKey, List<TValue>>
    {
        public void Add(TKey key, TValue value)
        {
            if (ContainsKey(key))
                base[key].Add(value);
            else
            {
                List<TValue> list = new List<TValue>();
                list.Add(value);
                base.Add(key, list);
            }
        }
    }
}
