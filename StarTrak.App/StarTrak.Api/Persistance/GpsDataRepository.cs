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
        private readonly ApplicationDbContext _applicationDbContext;

        public GpsDataRepository(ApplicationDbContext applicationDbContext)
        {
            _applicationDbContext = applicationDbContext;
        }

        public void Add(GpsData gpsData)
        {
            _applicationDbContext.Add(gpsData);
            _applicationDbContext.SaveChanges();
        }

        public async Task<IEnumerable<GpsData>> GetAllAsync()
        {
            return await _applicationDbContext.GpsData.OrderBy(o => o.Name).ToListAsync();
        }

        public IEnumerable<GpsData> GetAll()
        {
            return _applicationDbContext.GpsData.ToList();
        }

        public GpsData GetBy(Guid id)
        {
            return _applicationDbContext.Find<GpsData>(id);
        }

        public void Remove(Guid id)
        {
            var gpsData = GetBy(id);
            _applicationDbContext.Remove(gpsData);
        }

        public void Update(GpsData gpsData)
        {
            _applicationDbContext.Update(gpsData);
        }
    }
}
