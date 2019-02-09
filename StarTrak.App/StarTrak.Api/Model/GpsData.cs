using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace StarTrak.Api.Model
{
    public class GpsData
    {

        public Guid Id { get; set; }
        public string Name { get; set; }
        public DateTime GpsUTC { get; set; }
        public string NsIndicator { get; set; }
        public string Lon { get; set; }
        public string EwIndicator { get; set; }
        public string Fs { get; set; }
        public string NoSv { get; set; }
        public string Hdop { get; set; }
        public string Umsl { get; set; }
        public string AltRef { get; set; }
        public string Usep { get; set; }
        public string Msl { get; set; }
        public string MacAddress { get; set; }

    }
}
