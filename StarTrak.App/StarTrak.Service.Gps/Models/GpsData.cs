using System;
using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;

namespace StarTrak.Service.Gps.Models
{
    public class GpsData
    {
        [Key]
        [Required]
        [Column(TypeName = "uniqueidentifier")]
        public Guid Id { get; set; }
        [Required]
        public string ProtocolHeader { get; set; }
        [Required]
        [Column(TypeName = "datetime")]
        public DateTime GpsUtcDateTime { get; set; }
        [Required]
        public float NSIndicator { get; set; }
        [Required]
        public float Latitude { get; set; }
        [Required]
        public float EWIndicator { get; set; }
        [Required]
        public float Longitude { get; set; }
        public string FS { get; set; } 
        public string NoSv { get; set; }
        public string HDop { get; set; }
        public string UMsl { get; set; }
        public string Altitude { get; set; }
        public string USep { get; set; }
        public string MSL { get; set; }
        [Required]
        [Column(TypeName = "char(20)")]
        public string MacAddress { get; set; }
        public Guid PlayerId { get; set; }

    }
}