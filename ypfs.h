const char* dates_str = "^/Dates$";
const char* dates_year_str = "^/Dates/[0-9]{4}$";
const char* dates_year_month_str = "^/Dates/[0-9]{4}/(January|February|March|April|May|June|July|August|September|October|November|December)$";
const char* all_str = "^/All$";
const char* formats_str = "^/Formats$";
const char* formats_ext_str = "^/Formats/(jpg|jpeg|png|bmp|tiff|tif)$";
const char* search_str = "^/Search$";
const char* search_term_str = "^/Search/[A-Za-z0-9_.][A-Za-z0-9_.]?[A-Za-z0-9_.]?[A-Za-z0-9_.]?[A-Za-z0-9_.]?[A-Za-z0-9_.]?[A-Za-z0-9_.]?[A-Za-z0-9_.]?[A-Za-z0-9_.]?[A-Za-z0-9_.]?$";


regex_t dates_rx;
regex_t dates_year_rx;
regex_t dates_year_month_rx;
regex_t all_rx;
regex_t formats_rx;
regex_t formats_ext_rx;
regex_t search_rx;
regex_t search_term_rx;