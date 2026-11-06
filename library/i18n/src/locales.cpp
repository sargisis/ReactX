// The built-in locale data (from CLDR, trimmed to what rx-i18n formats) and
// the plural rules.
#include <cmath>
#include <string>
#include <string_view>

#include "rx/i18n.hpp"

namespace rx::i18n {

namespace {

constexpr const char* kNbsp = " ";

// "ru_RU.UTF-8", "ru-RU", "RU" -> "ru".
std::string baseLanguage(std::string_view lang) {
    std::string base;
    for (char c : lang) {
        if (c == '-' || c == '_' || c == '.' || c == '@') break;
        base += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    return base;
}

const Locale& english() {
    static const Locale l{
        .months = {"January", "February", "March", "April", "May", "June", "July", "August", "September",
                   "October", "November", "December"},
        .monthsShort = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
        .weekdays = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
        .weekdaysShort = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"},
        .firstWeekday = 0,
        .hour12 = true,
    };
    return l;
}

const Locale& russian() {
    static const Locale l{
        .decimal = ",",
        .group = kNbsp,
        .percent = std::string("{0}") + kNbsp + "%",
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{dd}.{MM}.{y}",
        .dateMedium = "{d} {MMM} {y} г.",
        .dateLong = "{d} {MMMM} {y} г.",
        .dateFull = "{EEEE}, {d} {MMMM} {y} г.",
        .months = {"января", "февраля", "марта", "апреля", "мая", "июня", "июля", "августа", "сентября", "октября",
                   "ноября", "декабря"},
        .monthsShort = {"янв.", "февр.", "мар.", "апр.", "мая", "июн.", "июл.", "авг.", "сент.", "окт.", "нояб.",
                        "дек."},
        .monthsAlone = {"январь", "февраль", "март", "апрель", "май", "июнь", "июль", "август", "сентябрь",
                        "октябрь", "ноябрь", "декабрь"},
        .weekdays = {"воскресенье", "понедельник", "вторник", "среда", "четверг", "пятница", "суббота"},
        .weekdaysShort = {"вс", "пн", "вт", "ср", "чт", "пт", "сб"},
        .relSeconds = "{n, plural, one {# секунду} few {# секунды} other {# секунд}}",
        .relMinutes = "{n, plural, one {# минуту} few {# минуты} other {# минут}}",
        .relHours = "{n, plural, one {# час} few {# часа} other {# часов}}",
        .relDays = "{n, plural, one {# день} few {# дня} other {# дней}}",
        .relWeeks = "{n, plural, one {# неделю} few {# недели} other {# недель}}",
        .relMonths = "{n, plural, one {# месяц} few {# месяца} other {# месяцев}}",
        .relYears = "{n, plural, one {# год} few {# года} other {# лет}}",
        .past = "{0} назад",
        .future = "через {0}",
        .now = "сейчас",
    };
    return l;
}

const Locale& ukrainian() {
    static const Locale l{
        .decimal = ",",
        .group = kNbsp,
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{dd}.{MM}.{y}",
        .dateMedium = "{d} {MMM} {y} р.",
        .dateLong = "{d} {MMMM} {y} р.",
        .dateFull = "{EEEE}, {d} {MMMM} {y} р.",
        .months = {"січня", "лютого", "березня", "квітня", "травня", "червня", "липня", "серпня", "вересня",
                   "жовтня", "листопада", "грудня"},
        .monthsShort = {"січ.", "лют.", "бер.", "квіт.", "трав.", "черв.", "лип.", "серп.", "вер.", "жовт.",
                        "лист.", "груд."},
        .monthsAlone = {"січень", "лютий", "березень", "квітень", "травень", "червень", "липень", "серпень",
                        "вересень", "жовтень", "листопад", "грудень"},
        .weekdays = {"неділя", "понеділок", "вівторок", "середа", "четвер", "пʼятниця", "субота"},
        .weekdaysShort = {"нд", "пн", "вт", "ср", "чт", "пт", "сб"},
        .relSeconds = "{n, plural, one {# секунду} few {# секунди} other {# секунд}}",
        .relMinutes = "{n, plural, one {# хвилину} few {# хвилини} other {# хвилин}}",
        .relHours = "{n, plural, one {# годину} few {# години} other {# годин}}",
        .relDays = "{n, plural, one {# день} few {# дні} other {# днів}}",
        .relWeeks = "{n, plural, one {# тиждень} few {# тижні} other {# тижнів}}",
        .relMonths = "{n, plural, one {# місяць} few {# місяці} other {# місяців}}",
        .relYears = "{n, plural, one {# рік} few {# роки} other {# років}}",
        .past = "{0} тому",
        .future = "через {0}",
        .now = "зараз",
    };
    return l;
}

const Locale& armenian() {
    static const Locale l{
        .decimal = ",",
        .group = kNbsp,
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{dd}.{MM}.{y}",
        .dateMedium = "{dd} {MMM}, {y} թ.",
        .dateLong = "{dd} {MMMM}, {y} թ.",
        .dateFull = "{y} թ. {MMMM} {d}, {EEEE}",
        .months = {"հունվարի", "փետրվարի", "մարտի", "ապրիլի", "մայիսի", "հունիսի", "հուլիսի", "օգոստոսի",
                   "սեպտեմբերի", "հոկտեմբերի", "նոյեմբերի", "դեկտեմբերի"},
        .monthsShort = {"հնվ", "փտվ", "մրտ", "ապր", "մյս", "հնս", "հլս", "օգս", "սեպ", "հոկ", "նոյ", "դեկ"},
        .monthsAlone = {"հունվար", "փետրվար", "մարտ", "ապրիլ", "մայիս", "հունիս", "հուլիս", "օգոստոս", "սեպտեմբեր",
                        "հոկտեմբեր", "նոյեմբեր", "դեկտեմբեր"},
        .weekdays = {"կիրակի", "երկուշաբթի", "երեքշաբթի", "չորեքշաբթի", "հինգշաբթի", "ուրբաթ", "շաբաթ"},
        .weekdaysShort = {"կիր", "երկ", "երք", "չրք", "հնգ", "ուր", "շբթ"},
        .relSeconds = "{n} վայրկյան",
        .relMinutes = "{n} րոպե",
        .relHours = "{n} ժամ",
        .relDays = "{n} օր",
        .relWeeks = "{n} շաբաթ",
        .relMonths = "{n} ամիս",
        .relYears = "{n} տարի",
        .past = "{0} առաջ",
        .future = "{0} անց",
        .now = "հիմա",
    };
    return l;
}

const Locale& german() {
    static const Locale l{
        .decimal = ",",
        .group = ".",
        .percent = std::string("{0}") + kNbsp + "%",
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{dd}.{MM}.{y}",
        .dateMedium = "{d}. {MMM} {y}",
        .dateLong = "{d}. {MMMM} {y}",
        .dateFull = "{EEEE}, {d}. {MMMM} {y}",
        .months = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober",
                   "November", "Dezember"},
        .monthsShort = {"Jan.", "Feb.", "März", "Apr.", "Mai", "Juni", "Juli", "Aug.", "Sept.", "Okt.", "Nov.",
                        "Dez."},
        .weekdays = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"},
        .weekdaysShort = {"So.", "Mo.", "Di.", "Mi.", "Do.", "Fr.", "Sa."},
        .relSeconds = "{n, plural, one {# Sekunde} other {# Sekunden}}",
        .relMinutes = "{n, plural, one {# Minute} other {# Minuten}}",
        .relHours = "{n, plural, one {# Stunde} other {# Stunden}}",
        .relDays = "{n, plural, one {# Tag} other {# Tagen}}",
        .relWeeks = "{n, plural, one {# Woche} other {# Wochen}}",
        .relMonths = "{n, plural, one {# Monat} other {# Monaten}}",
        .relYears = "{n, plural, one {# Jahr} other {# Jahren}}",
        .past = "vor {0}",
        .future = "in {0}",
        .now = "jetzt",
    };
    return l;
}

const Locale& french() {
    static const Locale l{
        .decimal = ",",
        .group = " ",
        .percent = "{0} %",
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{dd}/{MM}/{y}",
        .dateMedium = "{d} {MMM} {y}",
        .dateLong = "{d} {MMMM} {y}",
        .dateFull = "{EEEE} {d} {MMMM} {y}",
        .months = {"janvier", "février", "mars", "avril", "mai", "juin", "juillet", "août", "septembre", "octobre",
                   "novembre", "décembre"},
        .monthsShort = {"janv.", "févr.", "mars", "avr.", "mai", "juin", "juil.", "août", "sept.", "oct.", "nov.",
                        "déc."},
        .weekdays = {"dimanche", "lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi"},
        .weekdaysShort = {"dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam."},
        .relSeconds = "{n, plural, one {# seconde} other {# secondes}}",
        .relMinutes = "{n, plural, one {# minute} other {# minutes}}",
        .relHours = "{n, plural, one {# heure} other {# heures}}",
        .relDays = "{n, plural, one {# jour} other {# jours}}",
        .relWeeks = "{n, plural, one {# semaine} other {# semaines}}",
        .relMonths = "{n} mois",
        .relYears = "{n, plural, one {# an} other {# ans}}",
        .past = "il y a {0}",
        .future = "dans {0}",
        .now = "maintenant",
    };
    return l;
}

const Locale& spanish() {
    static const Locale l{
        .decimal = ",",
        .group = ".",
        .minGrouping = 2,
        .percent = std::string("{0}") + kNbsp + "%",
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{d}/{M}/{y}",
        .dateMedium = "{d} {MMM} {y}",
        .dateLong = "{d} de {MMMM} de {y}",
        .dateFull = "{EEEE}, {d} de {MMMM} de {y}",
        .months = {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre",
                   "noviembre", "diciembre"},
        .monthsShort = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sept", "oct", "nov", "dic"},
        .weekdays = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"},
        .weekdaysShort = {"dom", "lun", "mar", "mié", "jue", "vie", "sáb"},
        .relSeconds = "{n, plural, one {# segundo} other {# segundos}}",
        .relMinutes = "{n, plural, one {# minuto} other {# minutos}}",
        .relHours = "{n, plural, one {# hora} other {# horas}}",
        .relDays = "{n, plural, one {# día} other {# días}}",
        .relWeeks = "{n, plural, one {# semana} other {# semanas}}",
        .relMonths = "{n, plural, one {# mes} other {# meses}}",
        .relYears = "{n, plural, one {# año} other {# años}}",
        .past = "hace {0}",
        .future = "dentro de {0}",
        .now = "ahora",
    };
    return l;
}

const Locale& italian() {
    static const Locale l{
        .decimal = ",",
        .group = ".",
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{dd}/{MM}/{y}",
        .dateMedium = "{d} {MMM} {y}",
        .dateLong = "{d} {MMMM} {y}",
        .dateFull = "{EEEE} {d} {MMMM} {y}",
        .months = {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno", "luglio", "agosto", "settembre",
                   "ottobre", "novembre", "dicembre"},
        .monthsShort = {"gen", "feb", "mar", "apr", "mag", "giu", "lug", "ago", "set", "ott", "nov", "dic"},
        .weekdays = {"domenica", "lunedì", "martedì", "mercoledì", "giovedì", "venerdì", "sabato"},
        .weekdaysShort = {"dom", "lun", "mar", "mer", "gio", "ven", "sab"},
        .relSeconds = "{n, plural, one {# secondo} other {# secondi}}",
        .relMinutes = "{n, plural, one {# minuto} other {# minuti}}",
        .relHours = "{n, plural, one {# ora} other {# ore}}",
        .relDays = "{n, plural, one {# giorno} other {# giorni}}",
        .relWeeks = "{n, plural, one {# settimana} other {# settimane}}",
        .relMonths = "{n, plural, one {# mese} other {# mesi}}",
        .relYears = "{n, plural, one {# anno} other {# anni}}",
        .past = "{0} fa",
        .future = "tra {0}",
        .now = "ora",
    };
    return l;
}

// Brazilian Portuguese, like CLDR's "pt".
const Locale& portuguese() {
    static const Locale l{
        .decimal = ",",
        .group = ".",
        .currencyGap = kNbsp,
        .dateShort = "{dd}/{MM}/{y}",
        .dateMedium = "{d} de {MMM} de {y}",
        .dateLong = "{d} de {MMMM} de {y}",
        .dateFull = "{EEEE}, {d} de {MMMM} de {y}",
        .months = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro",
                   "outubro", "novembro", "dezembro"},
        .monthsShort = {"jan.", "fev.", "mar.", "abr.", "mai.", "jun.", "jul.", "ago.", "set.", "out.", "nov.",
                        "dez."},
        .weekdays = {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira",
                     "sábado"},
        .weekdaysShort = {"dom.", "seg.", "ter.", "qua.", "qui.", "sex.", "sáb."},
        .firstWeekday = 0,
        .relSeconds = "{n, plural, one {# segundo} other {# segundos}}",
        .relMinutes = "{n, plural, one {# minuto} other {# minutos}}",
        .relHours = "{n, plural, one {# hora} other {# horas}}",
        .relDays = "{n, plural, one {# dia} other {# dias}}",
        .relWeeks = "{n, plural, one {# semana} other {# semanas}}",
        .relMonths = "{n, plural, one {# mês} other {# meses}}",
        .relYears = "{n, plural, one {# ano} other {# anos}}",
        .past = "há {0}",
        .future = "em {0}",
        .now = "agora",
    };
    return l;
}

const Locale& polish() {
    static const Locale l{
        .decimal = ",",
        .group = kNbsp,
        .minGrouping = 2,
        .currencyAfter = true,
        .currencyGap = kNbsp,
        .dateShort = "{dd}.{MM}.{y}",
        .dateMedium = "{d} {MMM} {y}",
        .dateLong = "{d} {MMMM} {y}",
        .dateFull = "{EEEE}, {d} {MMMM} {y}",
        .months = {"stycznia", "lutego", "marca", "kwietnia", "maja", "czerwca", "lipca", "sierpnia", "września",
                   "października", "listopada", "grudnia"},
        .monthsShort = {"sty", "lut", "mar", "kwi", "maj", "cze", "lip", "sie", "wrz", "paź", "lis", "gru"},
        .monthsAlone = {"styczeń", "luty", "marzec", "kwiecień", "maj", "czerwiec", "lipiec", "sierpień", "wrzesień",
                        "październik", "listopad", "grudzień"},
        .weekdays = {"niedziela", "poniedziałek", "wtorek", "środa", "czwartek", "piątek", "sobota"},
        .weekdaysShort = {"niedz.", "pon.", "wt.", "śr.", "czw.", "pt.", "sob."},
        .relSeconds = "{n, plural, one {# sekundę} few {# sekundy} other {# sekund}}",
        .relMinutes = "{n, plural, one {# minutę} few {# minuty} other {# minut}}",
        .relHours = "{n, plural, one {# godzinę} few {# godziny} other {# godzin}}",
        .relDays = "{n, plural, one {# dzień} other {# dni}}",
        .relWeeks = "{n, plural, one {# tydzień} few {# tygodnie} other {# tygodni}}",
        .relMonths = "{n, plural, one {# miesiąc} few {# miesiące} other {# miesięcy}}",
        .relYears = "{n, plural, one {# rok} few {# lata} other {# lat}}",
        .past = "{0} temu",
        .future = "za {0}",
        .now = "teraz",
    };
    return l;
}

}  // namespace

const Locale& locale(std::string_view lang) {
    const std::string base = baseLanguage(lang);
    if (base == "ru") return russian();
    if (base == "uk") return ukrainian();
    if (base == "hy") return armenian();
    if (base == "de") return german();
    if (base == "fr") return french();
    if (base == "es") return spanish();
    if (base == "it") return italian();
    if (base == "pt") return portuguese();
    if (base == "pl") return polish();
    return english();
}

std::string_view pluralCategory(double n, std::string_view lang) {
    if (std::isnan(n) || std::isinf(n)) return "other";
    n = std::fabs(n);
    const bool whole = n == std::floor(n) && n < 9e15;  // CLDR "v = 0"
    const auto i = static_cast<long long>(std::floor(n < 9e15 ? n : 0));
    const long long i10 = i % 10, i100 = i % 100;
    const std::string base = baseLanguage(lang);

    // East Slavic: 1, 21 -> one; 2-4, 22-24 -> few; 0, 5-20, 25 -> many; fractions -> other.
    if (base == "ru" || base == "uk" || base == "be") {
        if (!whole) return "other";
        if (i10 == 1 && i100 != 11) return "one";
        if (i10 >= 2 && i10 <= 4 && (i100 < 12 || i100 > 14)) return "few";
        return "many";
    }
    if (base == "pl") {
        if (!whole) return "other";
        if (i == 1) return "one";
        if (i10 >= 2 && i10 <= 4 && (i100 < 12 || i100 > 14)) return "few";
        return "many";
    }
    if (base == "cs" || base == "sk") {
        if (!whole) return "many";
        if (i == 1) return "one";
        if (i >= 2 && i <= 4) return "few";
        return "other";
    }
    // 0 and 1 (and 1.5) are singular.
    if (base == "fr" || base == "hy" || base == "pt") return i == 0 || i == 1 ? "one" : "other";
    if (base == "ar") {
        if (!whole) return "other";
        if (i == 0) return "zero";
        if (i == 1) return "one";
        if (i == 2) return "two";
        if (i100 >= 3 && i100 <= 10) return "few";
        if (i100 >= 11) return "many";
        return "other";
    }
    if (base == "ja" || base == "zh" || base == "ko" || base == "vi" || base == "th" || base == "id")
        return "other";
    // English, German, Spanish, Italian and most others: only 1 is singular.
    return whole && i == 1 ? "one" : "other";
}

}  // namespace rx::i18n
