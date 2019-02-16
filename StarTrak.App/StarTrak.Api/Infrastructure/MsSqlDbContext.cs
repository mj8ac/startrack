using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.EntityFrameworkCore;
using StarTrak.Api.Model;

namespace StarTrak.Api.Infrastructure
{
    public class MsSqlDbContext : DbContext
    {
        public MsSqlDbContext(DbContextOptions<MsSqlDbContext> options) : base(options)
        {
            
        }

        public MsSqlDbContext()
        {
            
        }

        public DbSet<GpsData> GpsData { get; set; }
    }
}
