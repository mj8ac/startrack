using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Configuration;
using StarTrak.Api.Model;

namespace StarTrak.Api.Infrastructure
{
    public class ApplicationDbContext : DbContext
    {

        public ApplicationDbContext(DbContextOptions<ApplicationDbContext> options, IConfiguration configuration) : base(options)
        {
        }

        public ApplicationDbContext()
        {
            
        }

        public DbSet<GpsData> GpsData { get; set; }
    }
}
