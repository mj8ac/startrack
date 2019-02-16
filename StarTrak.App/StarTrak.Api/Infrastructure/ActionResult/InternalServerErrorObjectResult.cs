using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace StarTrak.Api.Infrastructure.ActionResult
{
    public class InternalServerErrorObjectResult : ObjectResult
    {
        public InternalServerErrorObjectResult(object value) : base(value)
        {
            StatusCode = StatusCodes.Status500InternalServerError;
        }
    }
}