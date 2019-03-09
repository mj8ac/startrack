using Microsoft.EntityFrameworkCore;
using StarTrak.Service.Gps.Models;

namespace StarTrak.Service.Gps.Persistence
{
    public class GpsDbContext : DbContext
    {
        public GpsDbContext(DbContextOptions<GpsDbContext> options) : base(options){ }

        public DbSet<GpsData> GpsData { get; set; }
    }
}