import Head from "@/components/Head";
import styles from "@/styles/Home.module.css";
import Header from "@/components/Header";
import { Button, Link, FormHelperText } from "@mui/material";
import { useForm, SubmitHandler } from "react-hook-form";
import useLoading, { DontClearLoading } from "@/hooks/useLoading";
import { createAccount } from "@/lib/api";
import { ErrorId } from "@/lib/apiTypes";
import { useRouter } from "next/router";
import EmailInput from "@/components/EmailInput";
import PasswordInput from "@/components/PasswordInput";
import UsernameInput from "@/components/UsernameInput";
import { useCallback } from "react";
import FormCard from "@/components/FormCard";
import { setHasAuth } from "@/lib/hasAuth";

type Inputs = {
  username: string;
  email: string;
  password: string;
};

// The home page
export default function HomePage() {
  const router = useRouter();

  const {
    register,
    handleSubmit,
    setError,
    formState: { errors },
  } = useForm<Inputs>({ mode: "onTouched" });

  const { loading, launch } = useLoading();

  const onSubmit: SubmitHandler<Inputs> = useCallback(
    (inputs) => {
      launch(async () => {
        try {
          const { type } = await createAccount(inputs);
          switch (type) {
            case "ok":
              setHasAuth();
              router.push("/chat");
              return DontClearLoading;
            case ErrorId.EmailExists:
              setError("email", {
                type: "value",
                message: "This email address is already in use.",
              });
              break;
            case ErrorId.UsernameExists:
              setError("username", {
                type: "value",
                message:
                  "This username is already in use. Please pick a different one.",
              });
              break;
          }
        } catch (err) {
          setError("root", {
            type: "value",
            message: "There was an unexpected error. Please try again later.",
          });
        }
      });
    },
    [router, setError, launch],
  );

  // return (
  //   <>
  //     <Head />
  //     <div className="flex flex-col">
  //       <Header />
  //       <div className={`${styles.bodycontainer} p-12`}>
  //         <div className="text-center pb-8">
  //           <p className="text-3xl p-3 m-0">欢迎来到</p>
  //           <p className="text-6xl p-3 m-0">💬 陆灵妖哔命理推理服务平台 💬</p>
  //           <p className="text-xl p-3 m-0">
  //             一款专属于潮汕人的命理大模型
  //           </p>
  //         </div>
  //         <div className="flex justify-center">
  //           <FormCard title="五行之道，就在其中">
  //             <form
  //               className="flex-1 flex flex-col"
  //               onSubmit={handleSubmit(onSubmit)}
  //             >
  //               <EmailInput
  //                 register={register}
  //                 name="email"
  //                 errorMessage={errors?.email?.message}
  //                 className="flex-1 pb-2"
  //               />
  //               <PasswordInput
  //                 register={register}
  //                 name="password"
  //                 errorMessage={errors?.password?.message}
  //                 className="flex-1 pb-2"
  //               />
  //               <UsernameInput
  //                 register={register}
  //                 name="username"
  //                 errorMessage={errors?.username?.message}
  //                 className="flex-1 pb-2"
  //               />
  //               {errors.root && (
  //                 <FormHelperText error>{errors.root.message}</FormHelperText>
  //               )}
  //               <div className="pt-8 flex justify-center">
  //                 <Button variant="contained" type="submit" disabled={loading}>
  //                   {loading ? "Creating account..." : "Create my account"}
  //                 </Button>
  //               </div>
  //               <div className="pt-8 flex justify-center">
  //                 <p className="p-0 m-0 text-sm">
  //                   Already have an account? Go to{" "}
  //                   <Link href="/login">Sign In</Link>
  //                 </p>
  //               </div>
  //             </form>
  //           </FormCard>
  //         </div>
  //       </div>
  //     </div>
  //   </>
  // );
  return (
    <>
      <Head />
      <div className="flex flex-col">
        <div className={`${styles.bodycontainer} p-12`}>
          <div className="text-center pb-8">
            <p className="text-3xl p-3 m-0">欢迎来到</p>
            <p className="text-6xl p-3 m-0">💬 陆灵妖哔命理推理服务平台 💬</p>
            <p className="text-xl p-3 m-0">
              一款专属于♥你♥的命理服务
            </p>
          </div>
          <div className="flex justify-center">
            <FormCard title="五行之道，就在其中">
              <form
                className="flex-1 flex flex-col"
                onSubmit={handleSubmit(onSubmit)}
              >
                <EmailInput
                  register={register}
                  name="email"
                  errorMessage={errors?.email?.message}
                  className="flex-1 pb-2"
                />
                <PasswordInput
                  register={register}
                  name="password"
                  errorMessage={errors?.password?.message}
                  className="flex-1 pb-2"
                />
                <UsernameInput
                  register={register}
                  name="username"
                  errorMessage={errors?.username?.message}
                  className="flex-1 pb-2"
                />
                {errors.root && (
                  <FormHelperText error>{errors.root.message}</FormHelperText>
                )}
                <div className="pt-8 flex justify-center">
                  <Button variant="contained" type="submit" disabled={loading} aria-label="Create my account">
                    {loading ? "注册账号中..." : "注册账号"}
                  </Button>
                </div>
                <div className="pt-8 flex justify-center">
                  <p className="p-0 m-0 text-sm">
                    已有账户? 立即{" "}
                    <Link href="/login">登陆</Link>
                  </p>
                </div>
              </form>
            </FormCard>
          </div>
        </div>
      </div>
    </>
  );
}
