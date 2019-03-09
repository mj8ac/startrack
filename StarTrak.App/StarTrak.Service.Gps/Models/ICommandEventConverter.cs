using StarTrak.Service.Gps.Events;

namespace StarTrak.Service.Gps.Models
{
    public interface ICommandEventConverter
    {
        GpsRecordedEvent CommandToEvent(GpsData gpsData);
    }
}