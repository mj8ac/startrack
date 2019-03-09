using System;
using Microsoft.EntityFrameworkCore.Migrations;

namespace StarTrak.Service.Gps.Migrations
{
    public partial class InitialDatabaseCreate : Migration
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "GpsData",
                columns: table => new
                {
                    Id = table.Column<Guid>(type: "uniqueidentifier", nullable: false),
                    ProtocolHeader = table.Column<string>(nullable: false),
                    GpsUtcDateTime = table.Column<DateTime>(type: "datetime", nullable: false),
                    NSIndicator = table.Column<float>(nullable: false),
                    Latitude = table.Column<float>(nullable: false),
                    EWIndicator = table.Column<float>(nullable: false),
                    Longitude = table.Column<float>(nullable: false),
                    FS = table.Column<string>(nullable: true),
                    NoSv = table.Column<string>(nullable: true),
                    HDop = table.Column<string>(nullable: true),
                    UMsl = table.Column<string>(nullable: true),
                    Altitude = table.Column<string>(nullable: true),
                    USep = table.Column<string>(nullable: true),
                    MSL = table.Column<string>(nullable: true),
                    MacAddress = table.Column<string>(type: "char(20)", nullable: false),
                    PlayerId = table.Column<Guid>(nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_GpsData", x => x.Id);
                });
        }

        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "GpsData");
        }
    }
}
