using StarTrak.Api.Model;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace StarTrak.Api.Persistance
{
    public interface IGpsDataRepository
    {
        void Add(GpsData gpsData);
        IEnumerable<GpsData> GetAll();
        GpsData GetBy(Guid id);
        void Remove(Guid id);
        void Update(GpsData gpsData);
    }
}
