using System;
using System.Collections.Generic;
using System.Linq;
using Microsoft.EntityFrameworkCore;
using StarTrak.Service.Gps.Models;

namespace StarTrak.Service.Gps.Persistence
{
    public class GpsDataRepository : IGpsRecordRepository
    {
        private GpsDbContext _context;

        public GpsDataRepository(GpsDbContext context)
        {
            _context = context;
        }
        public GpsData Add(GpsData gpsData)
        {
            _context.Add(gpsData);
            _context.SaveChanges();
            return gpsData;
        }

        public GpsData Update(GpsData gpsData)
        {
            _context.Entry(gpsData).State = EntityState.Modified;
            _context.SaveChanges();
            return gpsData;
        }

        public GpsData Get(Guid playerId, Guid recordId)
        {
            return _context.GpsData.FirstOrDefault(m => m.PlayerId == playerId && m.Id == recordId);
        }

        public GpsData Delete(Guid playerId, Guid recordId)
        {
            GpsData gpsData = Get(playerId, recordId);
            _context.Remove(gpsData);
            _context.SaveChanges();
            return gpsData;
        }

        public GpsData GetLatestForPlayer(Guid playerId)
        {
            GpsData gpsData = _context.GpsData
                .Where(m => m.PlayerId == playerId)
                .OrderBy(m => m.GpsUtcDateTime)
                .Last();
            return gpsData;
        }

        public ICollection<GpsData> AllForPlayer(Guid playerId)
        {
            return _context.GpsData
                .Where(m => m.PlayerId == playerId)
                .OrderBy(m => m.GpsUtcDateTime)
                .ToList();
        }

        public ICollection<GpsData> AllGpsData()
        {
            return _context.GpsData
                .OrderBy(m => m.GpsUtcDateTime)
                .ToList();
        }
    }
}