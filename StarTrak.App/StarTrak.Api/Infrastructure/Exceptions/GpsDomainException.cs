using System;

namespace StarTrak.Api.Infrastructure.Exceptions
{
    public class GpsDomainException : Exception
    {
        public GpsDomainException()
        { }

        public GpsDomainException(string message) : base(message)
        { }

        public GpsDomainException(string message, Exception innerException) : base(message, innerException)
        { }
    }
}