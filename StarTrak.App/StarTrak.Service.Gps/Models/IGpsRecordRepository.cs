using System;
using System.Collections;
using System.Collections.Generic;

namespace StarTrak.Service.Gps.Models
{
    public interface IGpsRecordRepository
    {
        GpsData Add(GpsData gpsData);
        GpsData Update(GpsData gpsData);
        GpsData Get(Guid playerId, Guid recordId);
        GpsData Delete(Guid playerId, Guid recordId);

        GpsData GetLatestForPlayer(Guid playerId);

        ICollection<GpsData> AllForPlayer(Guid playerId);

        ICollection<GpsData> AllGpsData();

    }
}