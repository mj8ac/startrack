using System;
using Newtonsoft.Json;

namespace StarTrak.Service.Gps.Events
{
    public class GpsRecordedEvent
    {
        public string Id { get; set; }
        public string ProtocolHeader { get; set; }
        public string GpsUtcDateTime { get; set; }
        public float NSIndicator { get; set; }
        public float Latitude { get; set; }
        public float EWIndicator { get; set; }
        public float Longitude { get; set; }
        public string FS { get; set; } 
        public string NoSv { get; set; }
        public string HDop { get; set; }
        public string UMsl { get; set; }
        public string Altitude { get; set; }
        public string USep { get; set; }
        public string MSL { get; set; }
        public string MacAddress { get; set; }
        public string PlayerId { get; set; }

        public string ToJson()
        {
            return JsonConvert.SerializeObject(this);
        }
    }
}