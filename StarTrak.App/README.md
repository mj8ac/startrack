## StarTrak Application
To run this project you will need
* .NET Core `2.2.103`
* RabbitMQ
* MSSQL Express

Will need to configure RabbitMQ to use the Managment Plugin [Tutorial can be found here](https://www.rabbitmq.com/management.html)

In the `StarTrak.Services.Gps` folder the `appsettings.json` will have this configuration for RabbitMQ

```json
  "amqp": {
    "username": "guest",
    "password": "guest",
    "hostname": "localhost",
    "uri": "amqp://localhost:5672/",
    "virtualhost": "/"
  }
```

before running `StarTrak.Services.Gps`

need to go into the folder and run

```
dotnet restore
dotnet ef database update
```

This will restore all packages to the project and run the migration to create the database and the table

```
dotnet run
```

To run the project and you can go to [https://localhost:5001/swagger/index.html](https://localhost:5001/swagger/index.html) this will show the Api version and the urls with differant actions

Any question just let me know