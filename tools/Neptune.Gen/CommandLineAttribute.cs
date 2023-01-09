using System;

namespace Neptune.Gen
{
    [AttributeUsage(AttributeTargets.Field)]
    public class CommandLineAttribute : Attribute
    {
        public string Name;
        public string ValueHint;
        public string Description;

        public CommandLineAttribute(string name, string description)
        {
            if (name.IndexOf('-') != -1)
                throw new Exception("Command line argument cannot contain '-'.");
            Name = name;
            ValueHint = "";
            Description = description;
        }

        public CommandLineAttribute(string name, string valueHint, string description)
        {
            if (name.IndexOf('-') != -1)
                throw new Exception("Command line argument cannot contain '-'.");
            Name = name;
            ValueHint = valueHint;
            Description = description;
        }
    }
}
