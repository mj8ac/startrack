using System.Globalization;
using StarTrak.Service.Gps.Events;

namespace StarTrak.Service.Gps.Models
{
    public class CommandEventConverter : ICommandEventConverter
    {
        public GpsRecordedEvent CommandToEvent(GpsData gpsData)
        {
            GpsRecordedEvent gpsRecordedEvent = new GpsRecordedEvent
            {
                Id = gpsData.Id.ToString(),
                ProtocolHeader = gpsData.ProtocolHeader,
                GpsUtcDateTime = gpsData.GpsUtcDateTime.ToString(CultureInfo.InvariantCulture), 
                NSIndicator  = gpsData.NSIndicator,
                Latitude  = gpsData.Latitude,
                EWIndicator  = gpsData.EWIndicator,
                Longitude  = gpsData.Longitude,
                FS   = gpsData.FS,
                NoSv  = gpsData.NoSv,
                HDop  = gpsData.HDop,
                UMsl  = gpsData.UMsl,
                Altitude  = gpsData.Altitude,
                USep  = gpsData.USep,
                MSL  = gpsData.MSL,
                MacAddress  = gpsData.MacAddress,
                PlayerId  = gpsData.PlayerId.ToString()
            };

            return gpsRecordedEvent;
        }
    }
}