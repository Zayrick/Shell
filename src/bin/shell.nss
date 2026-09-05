settings
{
	priority=1
	exclude.where = !process.is_explorer
	showdelay = 200
	// Options to allow modification of system items
	modify.remove.duplicate=1
	tip.enabled=true
}

// localization
$loc_path='imports\lang\'
$loc_language=sys.lang.name
$loc_region=loc_language + "-" + sys.lang.country
$loc_fallback=if(loc_language == "de", "de-DE",
             if(loc_language == "es", "es-ES",
             if(loc_language == "pt", "pt-BR",
             if(loc_language == "uk", "ua",
             if(loc_language == "nb" or loc_language == "nn", "no",
             if(loc_language == "zh",
                if(sys.lang.country == "TW" or sys.lang.country == "HK" or sys.lang.country == "MO", "zh-TW", "zh-CN"),
                loc_language))))))
import lang loc_path + "en.nss"
import lang if(path.exists(loc_path + loc_region + ".nss"),
               loc_path + loc_region + ".nss",
               if(path.exists(loc_path + loc_fallback + ".nss"),
                  loc_path + loc_fallback + ".nss"))

// or import lang 'imports/lang/en.nss'

import 'imports/theme.nss'
import 'imports/images.nss'
import 'imports/modify.nss'

menu(mode="multiple" title=loc.pin_unpin image=icon.pin)
{
}

menu(mode="multiple" title=title.more_options image=icon.more_options)
{
}

import 'imports/terminal.nss'
import 'imports/file-manage.nss'
import 'imports/develop.nss'
import 'imports/goto.nss'
import 'imports/taskbar.nss'
