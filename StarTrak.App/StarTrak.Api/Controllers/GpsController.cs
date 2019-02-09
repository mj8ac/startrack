using Microsoft.AspNetCore.Mvc;
using StarTrak.Api.Model;
using StarTrak.Api.Persistance;
using System.Collections.Generic;

namespace StarTrak.Api.Controllers
{
    [Route("api/[controller]")]
    [ApiController]
    public class GpsController : ControllerBase
    {
        private readonly IGpsDataRepository _gpsDataRepository;

        public GpsController(IGpsDataRepository gpsDataRepository)
        {
            _gpsDataRepository = gpsDataRepository;
        }

        /// <summary>
        /// ListGps get all the gps data 
        /// Will hav to look a getting this async
        /// </summary>
        /// <returns>all GpsData</returns>
        [HttpGet]
        public IEnumerable<GpsData> ListGps()
        {
            return _gpsDataRepository.GetAll();
        }
    }
}