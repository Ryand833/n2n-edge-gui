/*
  N2N Edge GUI - GUI Configuration utility for n2n edge
  Copyright (C) 2010  Ryan M. Dorn
  https://sourceforge.net/projects/n2nedgegui/
  $Id$

  This file is part of N2N Edge GUI.

  N2N Edge GUI is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  N2N Edge GUI is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with N2N Edge GUI.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include "net.h"

#pragma comment(lib, "iphlpapi.lib")

void get_mac_address(WCHAR* mac_address, WCHAR* guid)
{
  PIP_ADAPTER_ADDRESSES addresses = NULL;
  PIP_ADAPTER_ADDRESSES curr_addr;
  ULONG buf_len = 0;
  ULONG err = GetAdaptersAddresses(AF_INET, NULL, NULL, NULL, &buf_len);
  if (err == ERROR_BUFFER_OVERFLOW)
  {
    addresses = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, buf_len);
    err = GetAdaptersAddresses(AF_INET, NULL, NULL, addresses, &buf_len);
    if (err == ERROR_SUCCESS)
    {
      curr_addr = addresses;
      char* guid_ansi = NULL;
      guid_ansi = new char[wcslen(guid) + 1];
      size_t num_conv;
      wcstombs_s(&num_conv, guid_ansi, wcslen(guid) + 1, guid, _TRUNCATE);
      while (curr_addr != NULL)
      {
        if (_stricmp(curr_addr->AdapterName, guid_ansi))
        {
          curr_addr = curr_addr->Next;
          continue;
        }
        BYTE* a = curr_addr->PhysicalAddress;
        for (int i = 0; i < 5; i++)
        {
          wsprintf(mac_address, L"%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
        }
        break;
      }
      delete [] guid_ansi;
    }
    HeapFree(GetProcessHeap(), 0, addresses);
  }
}

void get_addresses(WCHAR* ip_address, WCHAR* mac_address)
{
  HKEY hkey_adapters, hkey_adapter, hkey_ip, hkey_ipg;
  DWORD i = 0;
  WCHAR net_key[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}";
  WCHAR tcpip_key[] = L"SYSTEM\\CurrentControlSet\\services\\Tcpip\\Parameters\\Interfaces";
  WCHAR subkey_name[128];
  WCHAR component_id[128];
  WCHAR guid[39];
  DWORD subkey_size = 128;
  DWORD component_size = 128 * sizeof(WCHAR);
  DWORD guid_size = 39 * sizeof(WCHAR);
  bool adapter_found = false;

  // Clear the addresses
  wsprintf(ip_address, L"<unknown>");
  wsprintf(mac_address, L"<unknown>");

  // First we need to find the TAP-Win32 adapter (identified by 'tap0901') and get its GUID
  RegOpenKeyEx(HKEY_LOCAL_MACHINE, net_key, NULL, KEY_READ, &hkey_adapters);
  while (RegEnumKeyEx(hkey_adapters, i, subkey_name, &subkey_size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
  {
    RegOpenKeyEx(hkey_adapters, subkey_name, NULL, KEY_READ, &hkey_adapter);
    RegQueryValueEx(hkey_adapter, L"ComponentId", NULL, NULL, (LPBYTE)component_id, &component_size);
    RegQueryValueEx(hkey_adapter, L"NetCfgInstanceId", NULL, NULL, (LPBYTE)guid, &guid_size);
    RegCloseKey(hkey_adapter);
    if (!wcscmp(component_id, L"tap0901"))
    {
      adapter_found = true;
      break;
    }
    i++;
    subkey_size = 128;
  }
  RegCloseKey(hkey_adapters);

  // If we have the guid, fetch the IP address from the registry
  if (adapter_found)
  {
    DWORD dhcp_enabled = 0;
    DWORD buf_size = sizeof(DWORD);
    DWORD ip_size = 16 * sizeof(WCHAR);
    RegOpenKeyEx(HKEY_LOCAL_MACHINE, tcpip_key, NULL, KEY_READ, &hkey_ip);
    RegOpenKeyEx(hkey_ip, guid, NULL, KEY_READ, &hkey_ipg);
    RegQueryValueEx(hkey_ipg, L"EnableDHCP", NULL, NULL, (LPBYTE)&dhcp_enabled, &buf_size);
    if (dhcp_enabled)
    {
      RegQueryValueEx(hkey_ipg, L"DhcpIPAddress", NULL, NULL, (LPBYTE)ip_address, &ip_size);
    }
    else
    {
      RegQueryValueEx(hkey_ipg, L"IPAddress", NULL, NULL, (LPBYTE)ip_address, &ip_size);
    }
    RegCloseKey(hkey_ipg);
    RegCloseKey(hkey_ip);

    get_mac_address(mac_address, guid);
  }
}

bool validate_ipv4_address(WCHAR* ip_address)
{
  WCHAR c;
  WCHAR octet_val[4];
  octet_val[0] = 0;
  int octets = 0;
  int c_octet_len = 0;
  for (int i = 0; i < (int)wcslen(ip_address); i++)
  {
    c = ip_address[i];
    if ((c < '0' || c > '9') && c != '.') return false;
    if (c == '.')
    {
      if (c_octet_len < 1 || c_octet_len > 3) return false;
      octets++;
      octet_val[c_octet_len] = 0;
      int int_octet_val = _wtoi(octet_val);
      if (int_octet_val < 0 || int_octet_val > 255) return false;
      if (octets > 3) return false;
      c_octet_len = 0;
      continue;
    }
    octet_val[c_octet_len] = c;
    c_octet_len++;
    if (c_octet_len > 3) return false;
  }
  octets++;
  if (c_octet_len < 1 || c_octet_len > 3 || octets != 4) return false;
  return true;
}

bool validate_mac_address(WCHAR* mac_address)
{
  if (wcslen(mac_address) != 17) return false;
  WCHAR mac_addr_lower[18];
  wcscpy_s(mac_addr_lower, 18, mac_address);
  _wcslwr_s(mac_addr_lower, 18);
  WCHAR* w;
  for (int i = 0; i < 6; i++)
  {
    w = mac_addr_lower + (i * 3);
    if ((w[0] < '0' || w[0] > '9') && (w[0] < 'a' || w[0] > 'f')) return false;
    if ((w[1] < '0' || w[1] > '9') && (w[1] < 'a' || w[1] > 'f')) return false;
    if (i != 5 && w[2] != ':') return false;
  }
  return true;
}

bool validate_number_range(WCHAR* num, int min, int max)
{
  int v = _wtoi(num);
  if (min != -1 && v < min) return false;
  if (max != -1 && v > max) return false;
  return true;
}