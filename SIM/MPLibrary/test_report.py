from ctypes import *
from TSMaster import *
u8 = c_uint8
pu8 = POINTER(c_uint8)
ppu8 = POINTER(pu8)
s8 = c_int8
ps8 = POINTER(c_int8)
pps8 = POINTER(ps8)
u16 = c_uint16
pu16 = POINTER(c_uint16)
ppu16 = POINTER(pu16)
s16 = c_int16
ps16 = POINTER(c_int16)
pps16 = POINTER(ps16)
u32 = c_uint32
pu32 = POINTER(c_uint32)
ppu32 = POINTER(pu32)
s32 = c_int32
ps32 = POINTER(c_int32)
pps32 = POINTER(ps32)
s64 = c_int64
ps64 = POINTER(c_int64)
pps64 = POINTER(ps64)
u64 = c_uint64
pu64 = POINTER(c_uint64)
ppu64 = POINTER(pu64)
double = c_double
pdouble = POINTER(c_double)
ppdouble = POINTER(pdouble)
pchar = c_char_p
ppchar = POINTER(c_char_p)
char = c_char
single = c_float
psingle = POINTER(c_float)
ppsingle = POINTER(psingle)
TObject = c_void_p
cbool = c_bool
pbool = POINTER(c_bool)
pvoid = c_void_p
ppvoid = POINTER(c_void_p)
size_t = c_size_t
psize_t = POINTER(size_t)
ppsize_t = POINTER(psize_t)
ppnative_int = ppsize_t
pnative_int = psize_t
native_int = size_t
_test_init = CFUNCTYPE(s32,pnative_int,)
def test_init(Handle):
    funcaddr,flag = app.retrieve_api_address('test_report.test_init')
    if funcaddr == 0:
        return None
    test_init_ = cast(funcaddr,_test_init)
    return test_init_(Handle)
_test_set_report_name = CFUNCTYPE(s32,native_int,pchar,)
def test_set_report_name(Handle,reportName):
    funcaddr,flag = app.retrieve_api_address('test_report.test_set_report_name')
    if funcaddr == 0:
        return None
    test_set_report_name_ = cast(funcaddr,_test_set_report_name)
    return test_set_report_name_(Handle,reportName)
_test_title = CFUNCTYPE(s32,native_int,pchar,pchar,pchar,pchar,)
def test_title(Handle,testgroup,testcase,image,testpurpose):
    funcaddr,flag = app.retrieve_api_address('test_report.test_title')
    if funcaddr == 0:
        return None
    test_title_ = cast(funcaddr,_test_title)
    return test_title_(Handle,testgroup,testcase,image,testpurpose)
_test_step = CFUNCTYPE(s32,native_int,pchar,pchar,)
def test_step(Handle,step,AMsg):
    funcaddr,flag = app.retrieve_api_address('test_report.test_step')
    if funcaddr == 0:
        return None
    test_step_ = cast(funcaddr,_test_step)
    return test_step_(Handle,step,AMsg)
_test_pass = CFUNCTYPE(s32,native_int,pchar,pchar,)
def test_pass(Handle,step,AMsg):
    funcaddr,flag = app.retrieve_api_address('test_report.test_pass')
    if funcaddr == 0:
        return None
    test_pass_ = cast(funcaddr,_test_pass)
    return test_pass_(Handle,step,AMsg)
_test_fail = CFUNCTYPE(s32,native_int,pchar,pchar,)
def test_fail(Handle,step,AMsg):
    funcaddr,flag = app.retrieve_api_address('test_report.test_fail')
    if funcaddr == 0:
        return None
    test_fail_ = cast(funcaddr,_test_fail)
    return test_fail_(Handle,step,AMsg)
_test_warning = CFUNCTYPE(s32,native_int,pchar,pchar,)
def test_warning(Handle,step,AMsg):
    funcaddr,flag = app.retrieve_api_address('test_report.test_warning')
    if funcaddr == 0:
        return None
    test_warning_ = cast(funcaddr,_test_warning)
    return test_warning_(Handle,step,AMsg)
_test_na = CFUNCTYPE(s32,native_int,pchar,pchar,)
def test_na(Handle,step,AMsg):
    funcaddr,flag = app.retrieve_api_address('test_report.test_na')
    if funcaddr == 0:
        return None
    test_na_ = cast(funcaddr,_test_na)
    return test_na_(Handle,step,AMsg)
_test_final = CFUNCTYPE(s32,native_int,)
def test_final(Handle):
    funcaddr,flag = app.retrieve_api_address('test_report.test_final')
    if funcaddr == 0:
        return None
    test_final_ = cast(funcaddr,_test_final)
    return test_final_(Handle)
_test_openissue = CFUNCTYPE(s32,native_int,pchar,)
def test_openissue(Handle,openissueinfo):
    funcaddr,flag = app.retrieve_api_address('test_report.test_openissue')
    if funcaddr == 0:
        return None
    test_openissue_ = cast(funcaddr,_test_openissue)
    return test_openissue_(Handle,openissueinfo)
_test_testinfo = CFUNCTYPE(s32,native_int,pchar,pchar,pchar,pchar,pchar,pchar,)
def test_testinfo(Handle,ProductName,HWVersion,SWVersion,DUTVersion,TestEngineer,ManufacturerName):
    funcaddr,flag = app.retrieve_api_address('test_report.test_testinfo')
    if funcaddr == 0:
        return None
    test_testinfo_ = cast(funcaddr,_test_testinfo)
    return test_testinfo_(Handle,ProductName,HWVersion,SWVersion,DUTVersion,TestEngineer,ManufacturerName)
_test_set_watermark = CFUNCTYPE(s32,native_int,pchar,)
def test_set_watermark(Handle,AWaterMark):
    funcaddr,flag = app.retrieve_api_address('test_report.test_set_watermark')
    if funcaddr == 0:
        return None
    test_set_watermark_ = cast(funcaddr,_test_set_watermark)
    return test_set_watermark_(Handle,AWaterMark)
_test_set_html_name_and_logo = CFUNCTYPE(s32,native_int,pchar,pchar,)
def test_set_html_name_and_logo(Handle,AHtmlName,ALogoName):
    funcaddr,flag = app.retrieve_api_address('test_report.test_set_html_name_and_logo')
    if funcaddr == 0:
        return None
    test_set_html_name_and_logo_ = cast(funcaddr,_test_set_html_name_and_logo)
    return test_set_html_name_and_logo_(Handle,AHtmlName,ALogoName)
_test_export = CFUNCTYPE(s32,pchar,pchar,)
def test_export(csvfilePath,HtmlFilePath):
    funcaddr,flag = app.retrieve_api_address('test_report.test_export')
    if funcaddr == 0:
        return None
    test_export_ = cast(funcaddr,_test_export)
    return test_export_(csvfilePath,HtmlFilePath)
_sddb_to_json = CFUNCTYPE(s32,pchar,pchar,)
def sddb_to_json(sddbfile,jsonfile):
    funcaddr,flag = app.retrieve_api_address('test_report.sddb_to_json')
    if funcaddr == 0:
        return None
    sddb_to_json_ = cast(funcaddr,_sddb_to_json)
    return sddb_to_json_(sddbfile,jsonfile)
