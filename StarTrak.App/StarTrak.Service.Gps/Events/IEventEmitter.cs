namespace StarTrak.Service.Gps.Events
{
    public interface IEventEmitter
    {
        void EmitGpsRecordedEvent(GpsRecordedEvent gpsRecordedEvent);
    }
}