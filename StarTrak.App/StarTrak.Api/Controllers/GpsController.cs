using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using StarTrak.Api.Persistance;
using System;
using System.Threading.Tasks;

namespace StarTrak.Api.Controllers
{
    [Route("api/[controller]")]
    [ApiController]
    public class GpsController : ControllerBase
    {
        private readonly IGpsDataRepository _gpsDataRepository;
        private readonly ILogger _logger;

        public GpsController(IGpsDataRepository gpsDataRepository, ILoggerFactory loggerFactory)
        {
            _gpsDataRepository = gpsDataRepository;
            _logger = loggerFactory.CreateLogger(nameof(GpsController));
        }

        /// <summary>
        /// GetAllGpsData gets all the gsp data async
        /// </summary>
        /// <returns>IEnumarble gpsData</returns>
        [HttpGet]
        public async Task<ActionResult> GetAllGpsData()
        {
            try
            {
                var model = await _gpsDataRepository.GetAllAsync();
                return Ok(model);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex.Message);
                return BadRequest();
            }
        }

        /// <summary>
        /// ListGps get all the gps data 
        /// Will hav to look a getting this async
        /// </summary>
        /// <returns>all GpsData</returns>
        //[HttpGet]
        //public IEnumerable<GpsData> ListGps()
        //{
        //    return _gpsDataRepository.GetAll();
        //}
    }
}