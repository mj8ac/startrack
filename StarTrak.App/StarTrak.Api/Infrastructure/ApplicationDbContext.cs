using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Configuration;
using StarTrak.Api.Model;

namespace StarTrak.Api.Infrastructure
{
    public class ApplicationDbContext : DbContext
    {

        public IConfiguration Configuration { get; }

        public ApplicationDbContext(DbContextOptions<ApplicationDbContext> options, IConfiguration configuration) : base(options)
        {
            Configuration = configuration;
        }

        protected override void OnConfiguring(DbContextOptionsBuilder optionsBuilder)
        {
            
            optionsBuilder.UseMySQL(Configuration.GetConnectionString("DefaultConnection"));
        }

        public DbSet<GpsData> GpsData { get; set; }
    }
}
