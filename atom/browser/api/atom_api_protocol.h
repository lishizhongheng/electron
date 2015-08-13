// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_API_ATOM_API_PROTOCOL_H_
#define ATOM_BROWSER_API_ATOM_API_PROTOCOL_H_

#include <string>
#include <map>
#include <vector>

#include "atom/browser/net/atom_url_request_job_factory.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "native_mate/arguments.h"
#include "native_mate/dictionary.h"
#include "native_mate/handle.h"
#include "native_mate/wrappable.h"

namespace net {
class URLRequest;
class URLRequestContextGetter;
}

namespace atom {

class AtomBrowserContext;
class AtomURLRequestJobFactory;

namespace api {

class Protocol : public mate::Wrappable {
 public:
  using Handler =
      base::Callback<void(const net::URLRequest*, v8::Local<v8::Value>)>;
  using CompletionCallback = base::Callback<void(v8::Local<v8::Value>)>;

  static mate::Handle<Protocol> Create(
      v8::Isolate* isolate, AtomBrowserContext* browser_context);

 protected:
  explicit Protocol(AtomBrowserContext* browser_context);

  // mate::Wrappable implementations:
  virtual mate::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate);

 private:
  // Possible errors.
  enum ProtocolError {
    PROTOCOL_OK,  // no error
    PROTOCOL_FAIL,  // operation failed, should never occur
    PROTOCOL_REGISTERED,
    PROTOCOL_NOT_REGISTERED,
    PROTOCOL_INTERCEPTED,
    PROTOCOL_NOT_INTERCEPTED,
  };

  // The protocol handler that will create a protocol handler for certain
  // request job.
  template<typename RequestJob>
  class CustomProtocolHandler
      : public net::URLRequestJobFactory::ProtocolHandler {
   public:
    CustomProtocolHandler(
        v8::Isolate* isolate,
        scoped_refptr<net::URLRequestContextGetter> request_context,
        const Handler& handler)
        : isolate_(isolate),
          request_context_(request_context),
          handler_(handler) {}
    ~CustomProtocolHandler() override {}

    net::URLRequestJob* MaybeCreateJob(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const override {
      RequestJob* request_job = new RequestJob(request, network_delegate);
      request_job->SetHandlerInfo(isolate_, request_context_, handler_);
      return request_job;
    }

   private:
    v8::Isolate* isolate_;
    scoped_refptr<net::URLRequestContextGetter> request_context_;
    Protocol::Handler handler_;

    DISALLOW_COPY_AND_ASSIGN(CustomProtocolHandler);
  };

  // Register schemes to standard scheme list.
  void RegisterStandardSchemes(const std::vector<std::string>& schemes);

  // Register the protocol with certain request job.
  template<typename RequestJob>
  void RegisterProtocol(const std::string& scheme,
                        const Handler& handler,
                        mate::Arguments* args) {
    CompletionCallback callback;
    args->GetNext(&callback);
    content::BrowserThread::PostTaskAndReplyWithResult(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&Protocol::RegisterProtocolInIO<RequestJob>,
                   base::Unretained(this), scheme, handler),
        base::Bind(&Protocol::OnIOCompleted,
                   base::Unretained(this), callback));
  }
  template<typename RequestJob>
  ProtocolError RegisterProtocolInIO(const std::string& scheme,
                                     const Handler& handler) {
    if (job_factory_->IsHandledProtocol(scheme))
      return PROTOCOL_REGISTERED;
    scoped_ptr<CustomProtocolHandler<RequestJob>> protocol_handler(
        new CustomProtocolHandler<RequestJob>(
            isolate(), request_context_getter_, handler));
    if (job_factory_->SetProtocolHandler(scheme, protocol_handler.Pass()))
      return PROTOCOL_OK;
    else
      return PROTOCOL_FAIL;
  }

  // Unregistered the protocol handler that handles |scheme|.
  void UnregisterProtocol(const std::string& scheme, mate::Arguments* args);
  ProtocolError UnregisterProtocolInIO(const std::string& scheme);

  // Convert error code to JS exception and call the callback.
  void OnIOCompleted(const CompletionCallback& callback, ProtocolError error);

  // Convert error code to string.
  std::string ErrorCodeToString(ProtocolError error);

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  AtomURLRequestJobFactory* job_factory_;  // weak ref

  DISALLOW_COPY_AND_ASSIGN(Protocol);
};

}  // namespace api

}  // namespace atom

#endif  // ATOM_BROWSER_API_ATOM_API_PROTOCOL_H_
