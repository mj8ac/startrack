using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Newtonsoft.Json;
using StarTrak.Service.Gps.Events;
using StarTrak.Service.Gps.Models;

namespace StarTrak.Service.Gps.Controllers
{
    [ApiVersion("1.0")]
    [Route("api/v{version:apiVersion}/gps")]
    [ApiController]
    public class GpsController : ControllerBase
    {
        private ICommandEventConverter _converter;
        private IEventEmitter _eventEmitter;
        private IGpsRecordRepository _repository;

        public GpsController(ICommandEventConverter converter, IGpsRecordRepository repository, IEventEmitter eventEmitter)
        {
            _repository = repository;
            _eventEmitter = eventEmitter;
            _converter = converter;
        }

        [HttpPost("{playerId}")]
        public IActionResult AddLocation(Guid playerId, [FromBody] GpsData gpsData)
        {
            gpsData.Id = Guid.NewGuid();
            gpsData.PlayerId = playerId;
            GpsRecordedEvent gpsRecordedEvent = _converter.CommandToEvent(gpsData);
            _eventEmitter.EmitGpsRecordedEvent(gpsRecordedEvent);
            _repository.Add(gpsData);
            return Created($"/gps/{playerId}/{gpsData.Id}", gpsData);
        }

        [HttpGet("{playerId}")]
        public IActionResult GetGpsDataForPlayer(Guid playerId)
        {
            return Ok(_repository.AllForPlayer(playerId));
        }


        [HttpGet("{playerId}/latest")]
        public IActionResult GetLatestForPlayer(Guid playerId) {
            return Ok(_repository.GetLatestForPlayer(playerId));
        }

        [HttpGet("all")]
        public IActionResult GetAll()
        {
            return Ok(_repository.AllGpsData());
        }
    }
}