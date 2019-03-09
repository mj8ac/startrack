using Microsoft.AspNetCore.Mvc;

namespace StarTrak.Service.Gps.Controllers
{
    public class HomeController : Controller
    {
        // GET
        public IActionResult Index()
        {
            return new RedirectResult("~/swagger");
        }
    }
}