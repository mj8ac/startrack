using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.EntityFrameworkCore;
using StarTrak.Api.Infrastructure;
using StarTrak.Api.Model;

namespace StarTrak.Api.Persistance
{
    public class GpsDataRepository : IGpsDataRepository
    {

        private readonly MsSqlDbContext _msSqlDbContext;

        public GpsDataRepository(MsSqlDbContext msSqlDbContext)
        {
            _msSqlDbContext = msSqlDbContext;
        }

        public void Add(GpsData gpsData)
        {
            _msSqlDbContext.Add(gpsData);
            _msSqlDbContext.SaveChanges();

        }

        public async Task<IEnumerable<GpsData>> GetAllAsync()
        {
            return await _msSqlDbContext.GpsData.OrderBy(o => o.Name).ToListAsync();
        }

        public IEnumerable<GpsData> GetAll()
        {
            return _msSqlDbContext.GpsData.ToList();
        }

        public GpsData GetBy(Guid id)
        {
            return _msSqlDbContext.Find<GpsData>(id);
        }

        public void Remove(Guid id)
        {
            var gpsData = GetBy(id);
            _msSqlDbContext.Remove(gpsData);
        }

        public void Update(GpsData gpsData)
        {
            _msSqlDbContext.Update(gpsData);
        }
    }
}
