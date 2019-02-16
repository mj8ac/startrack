using System;
using Microsoft.EntityFrameworkCore.Migrations;

namespace StarTrak.Api.Migrations
{
    public partial class InitialCreate : Migration
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "GpsData",
                columns: table => new
                {
                    Id = table.Column<Guid>(nullable: false),
                    Name = table.Column<string>(nullable: true),
                    GpsUTC = table.Column<DateTime>(nullable: false),
                    NsIndicator = table.Column<string>(nullable: true),
                    Lon = table.Column<string>(nullable: true),
                    EwIndicator = table.Column<string>(nullable: true),
                    Fs = table.Column<string>(nullable: true),
                    NoSv = table.Column<string>(nullable: true),
                    Hdop = table.Column<string>(nullable: true),
                    Umsl = table.Column<string>(nullable: true),
                    AltRef = table.Column<string>(nullable: true),
                    Usep = table.Column<string>(nullable: true),
                    Msl = table.Column<string>(nullable: true),
                    MacAddress = table.Column<string>(nullable: true)
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
