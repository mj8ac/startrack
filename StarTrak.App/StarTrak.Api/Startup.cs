using Autofac;
using Autofac.Extensions.DependencyInjection;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using StarTrak.Api.Infrastructure;
using StarTrak.Api.Persistance;
using Swashbuckle.AspNetCore.Swagger;
using System;

namespace StarTrak.Api
{
    public class Startup
    {
        private readonly ILogger _logger;

        public IConfiguration Configuration { get; }

        public Startup(IConfiguration configuration, ILogger<Startup> logger)
        {
            Configuration = configuration;
            _logger = logger;
        }

        // This method gets called by the runtime. Use this method to add services to the container.
        public IServiceProvider ConfigureServices(IServiceCollection services)
        {
            services
                .AddCustomeMVC(Configuration)
                .AddCustomeDbContext(Configuration, _logger)
                .AddSwagger();
            

            var container = new ContainerBuilder();
            container.Populate(services);
            return new AutofacServiceProvider(container.Build());
            
        }

        // This method gets called by the runtime. Use this method to configure the HTTP request pipeline.
        public void Configure(IApplicationBuilder app, IHostingEnvironment env, ILoggerFactory loggerFactory)
        {

            app.UseCors("CorsPolicy");

            app.UseMvcWithDefaultRoute();

            app.UseSwagger()
               .UseSwaggerUI(c =>
               {
                   c.SwaggerEndpoint("/swagger/v1/swagger.json", "StarTrak API V1");
               });
        }
    }

    public static class CustomExtensionMethods
    {
        public static IServiceCollection AddCustomeMVC(this IServiceCollection services, IConfiguration configuration)
        {
            services.AddMvc()
                .SetCompatibilityVersion(CompatibilityVersion.Version_2_2)
                .AddControllersAsServices();

            services.AddCors(options =>
            {
                options.AddPolicy("CorsPolicy",
                    builder => builder
                    .SetIsOriginAllowed((host) => true)
                    .AllowAnyMethod()
                    .AllowAnyHeader()
                    .AllowCredentials());
            });

            return services;
        }

        public static IServiceCollection AddCustomeDbContext(this IServiceCollection services, IConfiguration configuration, ILogger logger)
        {
            services.AddDbContext<ApplicationDbContext>();

            services.AddSingleton<IGpsDataRepository, GpsDataRepository>();
            logger.LogInformation("Added GpsDataRepository to services");

            return services;
        }

        public static IServiceCollection AddSwagger(this IServiceCollection services)
        {
            services.AddSwaggerGen(options =>
            {
                options.DescribeAllEnumsAsStrings();
                options.SwaggerDoc("v1", new Swashbuckle.AspNetCore.Swagger.Info
                {
                    Title = "StarTrak - StarTrak HTTP API",
                    Version = "v1",
                    Description = "The StarTrak Microservice HTTP API. This is a Data-Driven/CRUD microservice sample",
                    TermsOfService = "Terms Of Service",
                    Contact = new Contact { Name = "Ian Moore", Url = "http://twitter.com/moorewebuk" },
                });
            });

            return services;
        }
    }
}
