using System.Text;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using RabbitMQ.Client;

namespace StarTrak.Service.Gps.Events
{
    public class EventEmitter : IEventEmitter
    {
        private readonly ILogger _logger;
        private AMQPOptions _rabbitOptions;
        private ConnectionFactory _connectionFactory;
        public const string QUEUE_GPSRECORDED = "startrak-gps-data";

        public EventEmitter(ILogger<EventEmitter> logger, IOptions<AMQPOptions> rabbitOptions)
        {
            _logger = logger;
            _rabbitOptions = rabbitOptions.Value;

            _connectionFactory = new ConnectionFactory();

            _connectionFactory.UserName = _rabbitOptions.Username;
            _connectionFactory.Password = _rabbitOptions.Password;
            _connectionFactory.VirtualHost = _rabbitOptions.VirtualHost;
            _connectionFactory.HostName = _rabbitOptions.HostName;
            _connectionFactory.Uri = _rabbitOptions.Uri;

            _logger.LogInformation($"AMQP Event Emitter configured with URI {_rabbitOptions.Uri}");
        }

        public void EmitGpsRecordedEvent(GpsRecordedEvent gpsRecordedEvent)
        {
            using (IConnection conn = _connectionFactory.CreateConnection())
            {
                using (IModel channel = conn.CreateModel())
                {
                    channel.QueueDeclare(QUEUE_GPSRECORDED, false, false, false, null);
                    string jsonPayload = gpsRecordedEvent.ToJson();
                    var body = Encoding.UTF8.GetBytes(jsonPayload);
                    channel.BasicPublish("", QUEUE_GPSRECORDED, null, body);
                }
            }
        }
    }
}