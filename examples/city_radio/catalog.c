#include "catalog.h"
const radio_city radio_cities[RADIO_CITIES] = {
    {"北京", "Beijing", "BJ"},   {"上海", "Shanghai", "SH"},
    {"广州", "Guangzhou", "GZ"}, {"深圳", "Shenzhen", "SZ"},
    {"杭州", "Hangzhou", "HZ"},  {"长沙", "Changsha", "CS"},
    {"成都", "Chengdu", "CD"}};
const radio_station radio_stations[RADIO_STATIONS] = {
    {0, 339, "北京新闻广播", "Beijing News"},
    {0, 332, "北京音乐广播", "Beijing Music"},
    {1, 270, "上海新闻广播", "Shanghai News"},
    {1, 274, "动感101", "Shanghai FM101.7"},
    {2, 4848, "广州新闻广播", "Guangzhou News"},
    {2, 4955, "广州交通广播", "Guangzhou Traffic"},
    {3, 1270, "深圳先锋898", "Shenzhen News"},
    {3, 1271, "深圳飞扬971", "Shenzhen Music"},
    {4, 1133, "杭州交通91.8", "Hangzhou Traffic"},
    {4, 1163, "杭州西湖之声", "West Lake Radio"},
    {5, 4877, "长沙新闻广播", "Changsha News"},
    {5, 4237, "长沙城市之音", "Changsha City"},
    {6, 15318703, "成都 HIT FM", "Chengdu HIT FM"},
    {6, 15318317, "中国之声（全国备用）", "CNR (national backup)"}};
unsigned radio_city_matches(const char *prefix,
                            unsigned results[RADIO_CITIES]) {
  unsigned n = 0;
  for (unsigned i = 0; i < RADIO_CITIES; i++) {
    unsigned k = 0;
    while (prefix[k] && radio_cities[i].initials[k] == prefix[k])
      k++;
    if (!prefix[k])
      results[n++] = i;
  }
  return n;
}
void radio_remember(unsigned h[RADIO_HISTORY], unsigned *count,
                    unsigned station) {
  if (station >= RADIO_STATIONS)
    return;
  unsigned n = *count > RADIO_HISTORY ? RADIO_HISTORY : *count, i = 0;
  while (i < n && h[i] != station)
    i++;
  if (i == n && n < RADIO_HISTORY)
    n++;
  if (i >= RADIO_HISTORY)
    i = RADIO_HISTORY - 1;
  while (i) {
    h[i] = h[i - 1];
    i--;
  }
  h[0] = station;
  *count = n;
}
